#!/usr/bin/env python3
"""Сверка JSON-модели управляемой формы с Form.xml выгрузки конфигурации.

Модель строит сам просмотрщик (`v8reader_cli form-model`), эталон — XML,
который платформа пишет при выгрузке конфигурации в файлы
(`Configuration/…/Forms/<Форма>/Ext/Form.xml`). Скрипт нужен, чтобы
продолжать разбор свойств бинарной формы: он показывает, каких свойств
модель не отдаёт и где значения расходятся.

Пример:

    python tools/form_xml_compare.py \
        --cli build/v8reader_cli \
        --config "E:/work/dt_cf/1Cv8_bp.cf" \
        --dump "E:/work/dt_cf/bp/ConfigFiles.zip" \
        --object АвансовыйОтчет \
        --report out/compare-ao.txt

Без `--object` сверяются все формы дампа (долго: модель строится отдельным
запуском CLI на каждую форму, а конфигурация открывается заново).

`--dump` принимает zip выгрузки или каталог с ней.
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

# Теги элементов формы, которые в Form.xml являются элементами дерева.
ITEM_TAGS = {
    'UsualGroup', 'ButtonGroup', 'ColumnGroup', 'Pages', 'Page', 'Popup',
    'CommandBar', 'AutoCommandBar', 'ContextMenu',
    'InputField', 'LabelField', 'CheckBoxField', 'PictureField',
    'RadioButtonField', 'SpreadSheetDocumentField', 'TextDocumentField',
    'FormattedDocumentField', 'CalendarField', 'ChartField', 'GanttChartField',
    'PeriodField', 'ProgressBarField', 'TrackBarField', 'HTMLDocumentField',
    'PDFDocumentField', 'GeographicalSchemaField', 'DendrogramField',
    'GraphSchemaField', 'PlannerField', 'Table', 'Button', 'LabelDecoration',
    'PictureDecoration', 'GraphicalSchemaField', 'SearchStringAddition',
    'ViewStatusAddition', 'SearchControlAddition', 'ExtendedTooltip',
    'ButtonGroup', 'ColumnGroup', 'FormField',
}

# Служебные узлы формы, которые не являются полями/элементами дерева.
SERVICE_TAGS = {'ExtendedTooltip', 'ContextMenu'}

# Имена свойств, которые в модели называются иначе, чем в Form.xml.
ALIASES = {'Font': 'FontSpec', 'TitleFont': 'TitleFontSpec'}

# Свойства, которые сравниваются только по факту наличия: их значение в
# модели свёрнуто (вложенная запись) или выражено иначе (шрифт — набором
# атрибутов, картинка — именем общей картинки).
PRESENCE_ONLY = {'Font', 'FrameFont', 'TitleFont', 'ToolTipFont', 'Format',
                 'EditFormat', 'Border', 'Picture', 'HeaderPicture',
                 'ValuesPicture', 'RowsPicture', 'ChoiceList', 'Events',
                 'ChoiceParameters', 'ChoiceParameterLinks', 'TypeLink',
                 'AdditionSource', 'AutoCommandBar', 'SearchStringAddition',
                 'ViewStatusAddition', 'SearchControlAddition', 'CommandSet',
                 'UserSettingsGroup', 'Footer', 'FooterDataPath', 'FooterText',
                 'TitleHeight', 'ShowCheckBoxesInDropList',
                 'MultipleValueDataPath', 'NonselectedPictureText'}

# Каталоги выгрузки → вид объекта метаданных (для адресации в CLI:
# «Документ.АвансовыйОтчет»). Имена совпадают с typeTitle в MetadataCatalog.
CATEGORY_KINDS = {
    'Documents': 'Документ',
    'Catalogs': 'Справочник',
    'Reports': 'Отчет',
    'DataProcessors': 'Обработка',
    'CommonForms': 'Общая форма',
    'ExchangePlans': 'План обмена',
    'ChartsOfAccounts': 'План счетов',
    'ChartsOfCharacteristicTypes': 'План видов характеристик',
    'ChartsOfCalculationTypes': 'План видов расчета',
    'BusinessProcesses': 'Бизнес-процесс',
    'Tasks': 'Задача',
    'DocumentJournals': 'Журнал документов',
    'InformationRegisters': 'Регистр сведений',
    'AccumulationRegisters': 'Регистр накопления',
    'AccountingRegisters': 'Регистр бухгалтерии',
    'CalculationRegisters': 'Регистр расчета',
    'CommonCommands': 'Общая команда',
    'CommonModules': 'Общий модуль',
    'CommonTemplates': 'Общий макет',
    'CommonPictures': 'Общая картинка',
    'Enums': 'Перечисление',
    'Constants': 'Константа',
    'Roles': 'Роль',
}


def local_name(tag: str) -> str:
    return tag.rsplit('}', 1)[-1]


def localized_text(element: ET.Element) -> str:
    """Текст локализованной строки: берём `ru`, иначе первый непустой."""
    values = {}
    for item in element.iter():
        if local_name(item.tag) != 'item':
            continue
        lang = ''
        content = ''
        for child in item:
            name = local_name(child.tag)
            if name == 'lang':
                lang = (child.text or '').strip()
            elif name == 'content':
                content = (child.text or '').strip()
        if content:
            values.setdefault(lang or '?', content)
    return values.get('ru') or next(iter(values.values()), '')


class Node:
    __slots__ = ('tag', 'name', 'props', 'children')

    def __init__(self, tag: str, name: str, props: dict, children: list):
        self.tag = tag
        self.name = name
        self.props = props
        self.children = children

    def path(self, index: int = 0) -> str:
        base = '%s:%s' % (self.tag, self.name or '?')
        return base if index == 0 else '%s[%d]' % (base, index)


def parse_item(element: ET.Element) -> Node:
    tag = local_name(element.tag)
    props: dict[str, str | None] = {}
    children: list[Node] = []
    for child in element:
        child_tag = local_name(child.tag)
        if child_tag == 'ChildItems':
            children = [parse_item(item) for item in child
                        if local_name(item.tag) not in SERVICE_TAGS]
        elif child_tag == 'Title':
            props['Title'] = localized_text(child)
        elif child_tag == 'ToolTip':
            props['ToolTip'] = localized_text(child)
        elif child_tag in SERVICE_TAGS:
            continue
        elif len(child) > 0:
            # Вложенная запись (Format, Picture, Border, Events, …): в модели
            # она свёрнута в значение или отсутствует — сравниваем только факт.
            props[child_tag] = None
        else:
            props[child_tag] = (child.text or '').strip()
    return Node(tag, element.get('name') or '', props, children)


def parse_model_form(model: dict):
    items = [parse_model_item(node) for node in (model.get('childItemsRoot') or [])]
    auto_bar = model.get('autoCommandBar')
    if isinstance(auto_bar, dict) and auto_bar:
        items.append(parse_model_item(auto_bar))
    props = {}
    for key, value in (model.get('properties') or {}).items():
        if isinstance(value, (str, int, float, bool)):
            props[key] = str(value).lower() if isinstance(value, bool) else str(value)
    return items, props


def is_service_child(node: dict) -> bool:
    """Подсказка и контекстное меню — служебные записи, а не элементы дерева.

    В Form.xml это отдельные теги `ExtendedTooltip`/`ContextMenu`, а модель
    отдаёт их декорациями; чтобы деревья сопоставлялись, пропускаем и там,
    и там.
    """
    tag = node.get('tag') or ''
    name = node.get('name') or ''
    if tag in SERVICE_TAGS:
        return True
    return (name.endswith('РасширеннаяПодсказка')
            or name.endswith('ExtendedTooltip')
            or name.endswith('КонтекстноеМеню')
            or name.endswith('ContextMenu'))


def parse_model_item(node: dict) -> Node:
    props = {}
    for key, value in (node.get('properties') or {}).items():
        if isinstance(value, bool):
            props[key] = 'true' if value else 'false'
        elif isinstance(value, (str, int, float)):
            props[key] = str(value)
        elif isinstance(value, dict):
            props[key] = None
        else:
            props[key] = None
    children = [parse_model_item(child) for child in (node.get('childItems') or [])
                if not is_service_child(child)]
    return Node(node.get('tag') or '', node.get('name') or '', props, children)


class Report:
    def __init__(self):
        self.forms = 0
        self.xml_nodes = 0
        self.model_nodes = 0
        self.matched = 0
        self.structure_diffs: list[str] = []
        self.missing = collections.Counter()          # (tag, prop) → сколько
        self.missing_examples = collections.defaultdict(list)
        self.extra = collections.Counter()
        self.differs = collections.Counter()
        self.differs_examples = collections.defaultdict(list)
        self.value_kinds = collections.defaultdict(set)

    def note_missing(self, tag, prop, example, form):
        self.missing[(tag, prop)] += 1
        examples = self.missing_examples[(tag, prop)]
        if len(examples) < 3:
            examples.append('%s → %s' % (form, example))

    def note_differs(self, tag, prop, xml_value, model_value, example, form):
        self.differs[(tag, prop)] += 1
        examples = self.differs_examples[(tag, prop)]
        if len(examples) < 3:
            examples.append('%s → %s: xml=%r model=%r'
                            % (form, example, xml_value, model_value))

    def print_summary(self, stream):
        print('форм: %d, элементов: xml=%d model=%d, сопоставлено=%d'
              % (self.forms, self.xml_nodes, self.model_nodes, self.matched),
              file=stream)
        print('', file=stream)
        print('=== модель не отдаёт свойство (XML есть, модели нет) ===', file=stream)
        for (tag, prop), count in self.missing.most_common():
            examples = '; '.join(self.missing_examples[(tag, prop)])
            print('%-24s %-34s %4d   %s' % (tag, prop, count, examples), file=stream)
        print('', file=stream)
        print('=== значение расходится ===', file=stream)
        for (tag, prop), count in self.differs.most_common():
            examples = '; '.join(self.differs_examples[(tag, prop)])
            print('%-24s %-34s %4d   %s' % (tag, prop, count, examples), file=stream)
        print('', file=stream)
        print('=== модель отдаёт лишнее (в XML не пишется) ===', file=stream)
        for (tag, prop), count in self.extra.most_common(40):
            print('%-24s %-34s %4d' % (tag, prop, count), file=stream)
        if self.structure_diffs:
            print('', file=stream)
            print('=== расхождения структуры дерева ===', file=stream)
            for line in self.structure_diffs[:20]:
                print(line, file=stream)


def compare_nodes(xml_node: Node, model_node: Node, report: Report, form: str,
                  where: str):
    report.xml_nodes += 1
    report.model_nodes += 1
    if xml_node.tag != model_node.tag or (xml_node.name or '') != (model_node.name or ''):
        report.structure_diffs.append(
            '%s: xml=%s:%s model=%s:%s'
            % (where, xml_node.tag, xml_node.name, model_node.tag, model_node.name))
        return
    report.matched += 1
    where = '%s/%s' % (where, xml_node.name or xml_node.tag)

    for prop, value in xml_node.props.items():
        if prop.startswith(('ExtendedTooltip', 'ContextMenu')):
            continue
        model_prop = ALIASES.get(prop, prop)
        present = model_prop in model_node.props
        model_value = model_node.props.get(model_prop)
        if not present:
            report.note_missing(xml_node.tag, prop, where, form)
        elif prop in PRESENCE_ONLY or value is None or model_value is None:
            continue
        elif model_value != value:
            report.note_differs(xml_node.tag, prop, value, model_value, where, form)

    for prop in model_node.props:
        if prop in ALIASES.values():
            continue
        if prop not in xml_node.props:
            report.extra[(model_node.tag, prop)] += 1

    # Сопоставление детей: по порядку, с поиском по имени при рассинхронизации.
    index = 0
    for xml_child in xml_node.children:
        match = None
        for offset in range(0, len(model_node.children) - index):
            candidate = model_node.children[index + offset]
            if candidate.tag == xml_child.tag and candidate.name == xml_child.name:
                match = index + offset
                break
        if match is None:
            report.structure_diffs.append(
                '%s: в модели нет %s:%s' % (where, xml_child.tag, xml_child.name))
            report.xml_nodes += count_nodes(xml_child)
            continue
        index = match + 1
        compare_nodes(xml_child, model_node.children[match], report, form, where)

    for extra_child in model_node.children[index:]:
        report.structure_diffs.append(
            '%s: в модели лишний %s:%s' % (where, extra_child.tag, extra_child.name))
        report.model_nodes += count_nodes(extra_child)


def count_nodes(node: Node) -> int:
    return 1 + sum(count_nodes(child) for child in node.children)


def run_cli(cli: str, config: str, obj: str, form: str, out_path: str):
    """Возвращает (успех, stderr) запуска `form-model`."""
    command = [cli, 'form-model', config, obj, '--form', form, '-o', out_path]
    result = subprocess.run(command, capture_output=True, text=True,
                            encoding='utf-8', errors='replace')
    return (result.returncode == 0 and os.path.exists(out_path),
            (result.stderr or '') + (result.stdout or ''))


# Обычные формы 8.1/8.2 в контейнере тоже лежат в Forms, но корень у них
# другой: просмотрщик поддерживает только управляемые формы, и такие формы
# ожидаемо пропускаются.
ORDINARY_MARKER = 'ожидался корневой элемент управляемой формы'


def iter_dump_forms(dump: str, objects: set[str] | None):
    """Перебирает (вид, объект, форма, читалка XML) из выгрузки."""
    if os.path.isdir(dump):
        for root, _dirs, files in os.walk(dump):
            if 'Form.xml' not in files:
                continue
            rel = os.path.relpath(root, dump).replace('\\', '/')
            parts = rel.split('/')
            if len(parts) < 4 or parts[0] not in CATEGORY_KINDS:
                continue
            kind, obj, form = CATEGORY_KINDS[parts[0]], parts[1], parts[3]
            if objects and obj not in objects:
                continue
            path = os.path.join(root, 'Form.xml')
            yield kind, obj, form, lambda p=path: open(p, 'rb').read()
        return

    import zipfile
    with zipfile.ZipFile(dump) as archive:
        for name in archive.namelist():
            if not name.endswith('/Ext/Form.xml'):
                continue
            parts = name.split('/')
            if len(parts) < 5 or parts[0] not in CATEGORY_KINDS:
                continue
            kind, obj, form = CATEGORY_KINDS[parts[0]], parts[1], parts[3]
            if objects and obj not in objects:
                continue
            yield kind, obj, form, lambda n=name: archive.read(n)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--cli', required=True, help='путь к v8reader_cli')
    parser.add_argument('--config', required=True, help='файл .cf/.epf с формой')
    parser.add_argument('--dump', required=True, help='zip или каталог выгрузки')
    parser.add_argument('--object', action='append', default=None,
                        help='имя объекта метаданных (можно несколько)')
    parser.add_argument('--form', action='append', default=None,
                        help='ограничить формы с этими именами')
    parser.add_argument('--limit', type=int, default=0, help='сколько форм разобрать')
    parser.add_argument('--report', default='-', help='файл отчёта или - для stdout')
    args = parser.parse_args()

    objects = set(args.object) if args.object else None
    forms_filter = set(args.form) if args.form else None

    report = Report()
    temp_dir = tempfile.mkdtemp(prefix='qv8-compare-')
    model_path = os.path.join(temp_dir, 'model.json')
    processed = 0
    failures: list[str] = []
    ordinary: list[str] = []

    for kind, obj, form, read_xml in iter_dump_forms(args.dump, objects):
        if forms_filter and form not in forms_filter:
            continue
        if args.limit and processed >= args.limit:
            break
        target = '%s.%s' % (kind, obj)
        ok, cli_output = run_cli(args.cli, args.config, target, form, model_path)
        if not ok:
            label = '%s/%s' % (obj, form)
            if ORDINARY_MARKER in cli_output:
                ordinary.append(label)
            else:
                failures.append('%s: %s' % (label, cli_output.strip().splitlines()[-1:]))
            continue
        with open(model_path, encoding='utf-8') as handle:
            model = json.load(handle)
        payload = read_xml()
        xml_items, xml_props = parse_xml_form_bytes(payload)
        model_items, model_props = parse_model_form(model)
        report.forms += 1
        processed += 1
        label = '%s/%s' % (obj, form)

        for prop, value in xml_props.items():
            present = prop in model_props
            model_value = model_props.get(prop)
            if not present:
                report.note_missing('Form', prop, label, label)
            elif value is not None and model_value is not None and model_value != value:
                report.note_differs('Form', prop, value, model_value, label, label)

        index = 0
        for xml_item in xml_items:
            match = None
            for offset in range(0, len(model_items) - index):
                candidate = model_items[index + offset]
                if candidate.tag == xml_item.tag and candidate.name == xml_item.name:
                    match = index + offset
                    break
            if match is None:
                report.structure_diffs.append('%s: в модели нет %s:%s'
                                              % (label, xml_item.tag, xml_item.name))
                report.xml_nodes += count_nodes(xml_item)
                continue
            index = match + 1
            compare_nodes(xml_item, model_items[match], report, label, label)

    stream = sys.stdout if args.report == '-' else open(args.report, 'w', encoding='utf-8')
    try:
        report.print_summary(stream)
        if failures:
            print('', file=stream)
            print('=== CLI не построил модель ===', file=stream)
            for line in failures[:20]:
                print(line, file=stream)
        if ordinary:
            print('', file=stream)
            print('=== обычные формы (не поддерживаются, пропущены) ==='
                  ' %d' % len(ordinary), file=stream)
            for line in ordinary[:20]:
                print(line, file=stream)
    finally:
        if stream is not sys.stdout:
            stream.close()
    return 0


def parse_xml_form_bytes(payload: bytes):
    root = ET.fromstring(payload)
    form_props: dict[str, str] = {}
    items: list[Node] = []
    auto_bar: Node | None = None
    for child in root:
        tag = local_name(child.tag)
        if tag == 'ChildItems':
            items = [parse_item(item) for item in child]
        elif tag == 'AutoCommandBar':
            auto_bar = parse_item(child)
        elif tag in ('Attributes', 'Commands', 'Parameters', 'CommandInterface',
                     'Events', 'BaseAttributes'):
            continue
        else:
            form_props[tag] = (child.text or '').strip()
    if auto_bar is not None:
        items.append(auto_bar)
    return items, form_props


if __name__ == '__main__':
    sys.exit(main())
