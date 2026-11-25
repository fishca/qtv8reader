#!/usr/bin/env python3
"""Тестовая база контролов управляемых форм 1С и её выгрузка в файлы.

База — файловая информационная база ``E:\\work\\1cv83.db\\qtv8db`` (по
умолчанию) с конфигурацией ``КонтролыФорм``: одна обработка ``Контролы``,
в ней по форме на семейство контролов. Исходники — в репозитории:

    tests/fixtures/controlbase/skeleton/    скелет пустой конфигурации,
                                            снятый платформой (эталон формата)
    tests/fixtures/controlbase/spec/controls.json  спецификация форм и контролов
    tests/fixtures/controlbase/ConfigFiles/  сгенерированная выгрузка
    tests/fixtures/controlbase/1Cv8_controls.cf    контейнер для qtv8reader

Команды::

    python tools/control_base.py generate          # спецификация -> ConfigFiles
    python tools/control_base.py init              # создать файловую ИБ
    python tools/control_base.py push              # загрузить выгрузку в ИБ
    python tools/control_base.py pull              # выгрузить из ИБ в файлы
    python tools/control_base.py cf                # выгрузить в .cf
    python tools/control_base.py build             # generate + init + push + cf
    python tools/control_base.py check             # сверка с обратным дампом

Пакетные ключи конфигуратора (``CREATEINFOBASE``, ``/LoadConfigFromFiles``,
``/UpdateDBCfg``, ``/DumpConfigToFiles``, ``/DumpCfg``, ``/Out``,
``/DisableStartupDialogs``) описаны в документации 1С «Пакетный режим
конфигуратора».
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import uuid
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape, quoteattr

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURE = os.path.join(REPO, "tests", "fixtures", "controlbase")
SKELETON = os.path.join(FIXTURE, "skeleton")
SPEC_FILE = os.path.join(FIXTURE, "spec", "controls.json")
CONFIG_FILES = os.path.join(FIXTURE, "ConfigFiles")
CF_FILE = os.path.join(FIXTURE, "1Cv8_controls.cf")
VOCAB_FILE = os.path.join(FIXTURE, "vocab.json")
DEFAULT_BASE = r"E:\work\1cv83.db\qtv8db"
LOG_DIR = os.path.join(REPO, "out", "controlbase")

PLATFORM_ROOTS = (r"C:\Program Files\1cv8", r"C:\Program Files (x86)\1cv8",
                  r"E:\1cv8", r"C:\1cv8")

UI_NS = ("http://v8.1c.ru/8.3/MDClasses", "http://v8.1c.ru/8.2/managed-application/core",
         "http://v8.1c.ru/8.1/data/enterprise/current-config",
         "http://v8.1c.ru/8.2/managed-application/cmi",
         "http://v8.1c.ru/8.1/data/enterprise",
         "http://v8.1c.ru/8.2/managed-application/logform",
         "http://v8.1c.ru/8.1/data/ui/style",
         "http://v8.1c.ru/8.1/data/ui/fonts/system",
         "http://v8.1c.ru/8.1/data/core", "http://v8.1c.ru/8.1/data/ui",
         "http://v8.1c.ru/8.1/data/ui/colors/web",
         "http://v8.1c.ru/8.1/data/ui/colors/windows",
         "http://v8.1c.ru/8.3/xcf/enums", "http://v8.1c.ru/8.3/xcf/predef",
         "http://v8.1c.ru/8.3/xcf/readable",
         "http://www.w3.org/2001/XMLSchema",
         "http://www.w3.org/2001/XMLSchema-instance")

MD_PREFIXES = (
    ("", "http://v8.1c.ru/8.3/MDClasses"),
    ("app", "http://v8.1c.ru/8.2/managed-application/core"),
    ("cfg", "http://v8.1c.ru/8.1/data/enterprise/current-config"),
    ("cmi", "http://v8.1c.ru/8.2/managed-application/cmi"),
    ("ent", "http://v8.1c.ru/8.1/data/enterprise"),
    ("lf", "http://v8.1c.ru/8.2/managed-application/logform"),
    ("style", "http://v8.1c.ru/8.1/data/ui/style"),
    ("sys", "http://v8.1c.ru/8.1/data/ui/fonts/system"),
    ("v8", "http://v8.1c.ru/8.1/data/core"),
    ("v8ui", "http://v8.1c.ru/8.1/data/ui"),
    ("web", "http://v8.1c.ru/8.1/data/ui/colors/web"),
    ("win", "http://v8.1c.ru/8.1/data/ui/colors/windows"),
    ("xen", "http://v8.1c.ru/8.3/xcf/enums"),
    ("xpr", "http://v8.1c.ru/8.3/xcf/predef"),
    ("xr", "http://v8.1c.ru/8.3/xcf/readable"),
    ("xs", "http://www.w3.org/2001/XMLSchema"),
    ("xsi", "http://www.w3.org/2001/XMLSchema-instance"),
)

NAME_SPACE = uuid.UUID("6b0f7e2c-1f4e-5b3a-9c7d-2a5f0e4c8b11")


# --------------------------------------------------------------------------
# Общие помощники


def deterministic_uuid(*parts: str) -> str:
    return str(uuid.uuid5(NAME_SPACE, "/".join(parts)))


LOGFORM_PREFIXES = (
    ("", "http://v8.1c.ru/8.3/xcf/logform"),
    ("app", "http://v8.1c.ru/8.2/managed-application/core"),
    ("cfg", "http://v8.1c.ru/8.1/data/enterprise/current-config"),
    ("dcscor", "http://v8.1c.ru/8.1/data-composition-system/core"),
    ("dcssch", "http://v8.1c.ru/8.1/data-composition-system/schema"),
    ("dcsset", "http://v8.1c.ru/8.1/data-composition-system/settings"),
    ("ent", "http://v8.1c.ru/8.1/data/enterprise"),
    ("lf", "http://v8.1c.ru/8.2/managed-application/logform"),
    ("style", "http://v8.1c.ru/8.1/data/ui/style"),
    ("sys", "http://v8.1c.ru/8.1/data/ui/fonts/system"),
    ("v8", "http://v8.1c.ru/8.1/data/core"),
    ("v8ui", "http://v8.1c.ru/8.1/data/ui"),
    ("web", "http://v8.1c.ru/8.1/data/ui/colors/web"),
    ("win", "http://v8.1c.ru/8.1/data/ui/colors/windows"),
    ("xr", "http://v8.1c.ru/8.3/xcf/readable"),
    ("xs", "http://www.w3.org/2001/XMLSchema"),
    ("xsi", "http://www.w3.org/2001/XMLSchema-instance"),
)


def namespace_attrs(prefixes=None) -> str:
    return " ".join('%s="%s"' % (("xmlns:" + prefix) if prefix else "xmlns", uri)
                    for prefix, uri in (prefixes or MD_PREFIXES))


def write_text(path: str, text: str) -> None:
    """Запись в формате платформы: UTF-8 с BOM, переводы строк CRLF."""
    os.makedirs(os.path.dirname(path), exist_ok=True)
    data = text.replace("\r\n", "\n").replace("\n", "\r\n")
    with open(path, "wb") as handle:
        handle.write(b"\xef\xbb\xbf")
        handle.write(data.encode("utf-8"))


def title_xml(text: str, depth: int, element: str = "Title") -> str:
    pad = "\t" * depth
    return ("%s<%s>\n%s\t<v8:item>\n%s\t\t<v8:lang>ru</v8:lang>\n"
            "%s\t\t<v8:content>%s</v8:content>\n%s\t</v8:item>\n%s</%s>"
            % (pad, element, pad, pad, pad, escape(text), pad, pad, element))


def emit_command(command: dict, index: int) -> str:
    """Команда формы так, как её пишет платформа: Title, ToolTip, Action, CurrentRowUse.

    Кнопка без команды в конфигураторе не рисуется вовсе, поэтому кнопки в
    фикстуре получают настоящие команды формы, а к модулю формы — их
    обработчики (``Action`` — имя процедуры).
    """
    name = command["name"]
    lines = ['\t\t<Command name=%s id="%d">' % (quoteattr(name), index)]
    if command.get("title"):
        lines.append(title_xml(str(command["title"]), 3))
    if command.get("tooltip"):
        lines.append(title_xml(str(command["tooltip"]), 3, "ToolTip"))
    lines.append("\t\t\t<Action>%s</Action>" % escape(
        str(command.get("action") or name)))
    lines.append("\t\t\t<CurrentRowUse>%s</CurrentRowUse>"
                 % (command.get("currentRowUse") or "DontUse"))
    lines.append("\t\t</Command>")
    return "\n".join(lines)


def emit_form_module(form: dict) -> str:
    """Модуль формы с обработчиками команд: без них ``Action`` никуда не ведёт."""
    commands = form.get("commands") or []
    if not commands:
        return ""
    blocks = []
    for command in commands:
        handler = str(command.get("action") or command["name"])
        blocks.append("&НаКлиенте\nПроцедура %s(Команда)\n\t\nКонецПроцедуры"
                      % handler)
    return "\n\n".join(blocks) + "\n"


def value_text(value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def pick_platform(explicit: str | None, version: str | None = None) -> str:
    """Путь к 1cv8.exe.

    Версия берётся из спецификации (эталоны снимаются той же платформой, что
    записана в манифесте), явный ключ ``--platform`` её перекрывает.
    """
    if explicit:
        if os.path.isfile(explicit):
            return explicit
        candidate = os.path.join(explicit, "bin", "1cv8.exe")
        if os.path.isfile(candidate):
            return candidate
        raise SystemExit("не найден 1cv8.exe: " + explicit)
    found = []
    for root in PLATFORM_ROOTS:
        if not os.path.isdir(root):
            continue
        for name in os.listdir(root):
            exe = os.path.join(root, name, "bin", "1cv8.exe")
            if os.path.isfile(exe):
                found.append((name, exe))
    if not found:
        raise SystemExit("платформа 1С не найдена; укажите --platform")
    if version:
        for name, exe in found:
            if name == version:
                return exe
        print("  предупреждение: платформа %s не найдена, беру самую новую"
              % version)
    with_version = [item for item in found if item[0][0].isdigit()]
    pool = with_version or found
    pool.sort(key=lambda item: [int(p) for p in item[0].split(".") if p.isdigit()])
    return pool[-1][1]


def platform_for(args) -> str:
    version = None
    try:
        version = load_spec(args.spec).get("platform")
    except (OSError, ValueError):
        pass
    return pick_platform(args.platform, version)


def read_log(path: str) -> str:
    if not os.path.isfile(path):
        return ""
    data = open(path, "rb").read()
    if data.startswith(b"\xef\xbb\xbf"):
        return data[3:].decode("utf-8", "replace")
    for encoding in ("utf-8", "cp1251"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", "replace")


def run_batch(executable: str, args: list, log_path: str, timeout: int = 900) -> int:
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    if os.path.isfile(log_path):
        os.remove(log_path)
    command = [executable] + args + ["/Out", log_path, "/DisableStartupDialogs"]
    print("+ " + " ".join(command))
    started = time.time()
    result = subprocess.run(command, capture_output=True, text=True,
                            encoding="utf-8", errors="replace", timeout=timeout)
    log = read_log(log_path).strip()
    print("  exit=%d, %.1fs" % (result.returncode, time.time() - started))
    if log:
        for line in log.splitlines()[:40]:
            print("  | " + line)
    if result.returncode != 0 and not log:
        print("  stderr: " + (result.stderr or result.stdout or "").strip())
    return result.returncode


# --------------------------------------------------------------------------
# Спецификация и генерация выгрузки


def load_spec(path: str = SPEC_FILE) -> dict:
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def load_vocab() -> dict:
    if not os.path.isfile(VOCAB_FILE):
        return {}
    with open(VOCAB_FILE, encoding="utf-8") as handle:
        data = json.load(handle)
    return {tag: entry.get("order", []) for tag, entry in data.get("tags", {}).items()}


def order_props(tag: str, props: dict, vocab: dict) -> list:
    order = vocab.get(tag, [])
    known = [name for name in order if name in props]
    rest = [name for name in props if name not in known]
    return known + rest


def emit_prop(name: str, value, depth: int, single: bool = True) -> str:
    pad = "\t" * depth
    if isinstance(value, bool):
        value = value_text(value)
    if isinstance(value, (int, float, str)):
        return "%s<%s>%s</%s>" % (pad, name, escape(str(value)), name)
    if isinstance(value, dict):
        if "title" in value and name == "Title":
            return title_xml(str(value["title"]), depth)
        if "xml" in value:
            return "\n".join(pad + line for line in str(value["xml"]).split("\n"))
        raise SystemExit("свойство %s: неизвестный вид значения %r" % (name, value))
    raise SystemExit("свойство %s: неподдерживаемое значение %r" % (name, value))


def emit_element(element: dict, depth: int, vocab: dict, ids: dict,
                 warnings: list) -> str:
    tag = element["tag"]
    pad = "\t" * depth
    name = element.get("name", "")
    if not name:
        raise SystemExit("элемент %s без имени" % tag)
    ids["next"] += 1
    element_id = element.get("id", ids["next"])
    if element.get("id"):
        ids["next"] = max(ids["next"], int(element["id"]))
    attrs = ' name=%s id="%s"' % (quoteattr(name), element_id)
    props = dict(element.get("props") or {})
    children = element.get("children") or []
    lines = ["%s<%s%s>" % (pad, tag, attrs)]
    for prop in order_props(tag, props, vocab):
        if prop not in vocab.get(tag, []):
            warnings.append("%s.%s: свойство не встречалось в корпусе" % (tag, prop))
        lines.append(emit_prop(prop, props[prop], depth + 1))
    if children:
        lines.append("%s\t<ChildItems>" % pad)
        for child in children:
            lines.append(emit_element(child, depth + 2, vocab, ids, warnings))
        lines.append("%s\t</ChildItems>" % pad)
    lines.append("%s</%s>" % (pad, tag))
    return "\n".join(lines)


def type_xml(kind: str, spec: dict, depth: int) -> str:
    pad = "\t" * depth
    if kind == "string":
        return ("%s<Type>\n%s\t<v8:Type>xs:string</v8:Type>\n"
                "%s\t<v8:StringQualifiers>\n"
                "%s\t\t<v8:Length>%d</v8:Length>\n"
                "%s\t\t<v8:AllowedLength>Variable</v8:AllowedLength>\n"
                "%s\t</v8:StringQualifiers>\n%s</Type>"
                % (pad, pad, pad, pad, int(spec.get("length", 40)), pad, pad, pad))
    if kind == "number":
        return ("%s<Type>\n%s\t<v8:Type>xs:decimal</v8:Type>\n"
                "%s\t<v8:NumberQualifiers>\n"
                "%s\t\t<v8:Digits>%d</v8:Digits>\n"
                "%s\t\t<v8:FractionDigits>%d</v8:FractionDigits>\n"
                "%s\t\t<v8:AllowedSign>Any</v8:AllowedSign>\n"
                "%s\t</v8:NumberQualifiers>\n%s</Type>"
                % (pad, pad, pad, pad, int(spec.get("digits", 10)), pad,
                   int(spec.get("fraction", 2)), pad, pad, pad))
    if kind == "date":
        fractions = {"date": "Date", "datetime": "DateTime", "time": "Time"}
        return ("%s<Type>\n%s\t<v8:Type>xs:dateTime</v8:Type>\n"
                "%s\t<v8:DateQualifiers>\n"
                "%s\t\t<v8:DateFractions>%s</v8:DateFractions>\n"
                "%s\t</v8:DateQualifiers>\n%s</Type>"
                % (pad, pad, pad, pad, fractions.get(spec.get("fractions", "date")),
                   pad, pad))
    if kind == "boolean":
        return "%s<Type>\n%s\t<v8:Type>xs:boolean</v8:Type>\n%s</Type>" % (pad, pad, pad)
    if kind == "valuetable":
        return ("%s<Type>\n%s\t<v8:Type>v8:ValueTable</v8:Type>\n%s</Type>"
                % (pad, pad, pad))
    raise SystemExit("неизвестный тип реквизита: " + kind)


def emit_attribute(attribute: dict, depth: int, ids: dict) -> str:
    pad = "\t" * depth
    ids["attr"] += 1
    name = attribute["name"]
    kind = attribute.get("type", "string")
    lines = ['%s<Attribute name=%s id="%d">' % (pad, quoteattr(name), ids["attr"])]
    if attribute.get("title"):
        lines.append(title_xml(attribute["title"], depth + 1))
    lines.append(type_xml(kind, attribute, depth + 1))
    if kind == "valuetable":
        lines.append("%s\t<Columns>" % pad)
        for index, column in enumerate(attribute.get("columns", []), start=1):
            lines.append('%s\t\t<Column name=%s id="%d">'
                         % (pad, quoteattr(column["name"]), index))
            if column.get("title"):
                lines.append(title_xml(column["title"], depth + 3))
            lines.append(type_xml(column.get("type", "string"), column, depth + 3))
            lines.append("%s\t\t</Column>" % pad)
        lines.append("%s\t</Columns>" % pad)
    if attribute.get("main"):
        lines.append("%s\t<MainAttribute>true</MainAttribute>" % pad)
    lines.append("%s</Attribute>" % pad)
    return "\n".join(lines)


def emit_form_xml(form: dict, vocab: dict, warnings: list) -> str:
    ids = {"next": 0, "attr": 0}
    nested = [element for element in form.get("elements", [])
              if element.get("tag") not in ("AutoCommandBar",)]
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<Form %s version="2.20">' % namespace_attrs(LOGFORM_PREFIXES)]
    # Свойства самой формы (заголовок, автозаголовок, положение панели) идут
    # до AutoCommandBar — так их пишет и сама платформа.
    for prop in form.get("props") or {}:
        lines.append(emit_prop(prop, form["props"][prop], 1))
    bar_props = form.get("autoCommandBar") or {}
    if bar_props:
        lines.append('\t<AutoCommandBar name="ФормаКоманднаяПанель" id="-1">')
        for prop in bar_props:
            lines.append(emit_prop(prop, bar_props[prop], 2))
        lines.append("\t</AutoCommandBar>")
    else:
        lines.append('\t<AutoCommandBar name="ФормаКоманднаяПанель" id="-1"/>')
    lines.append("\t<ChildItems>")
    for element in nested:
        lines.append(emit_element(element, 2, vocab, ids, warnings))
    lines.append("\t</ChildItems>")
    attributes = form.get("attributes") or []
    if attributes:
        lines.append("\t<Attributes>")
        for attribute in attributes:
            lines.append(emit_attribute(attribute, 2, ids))
        lines.append("\t</Attributes>")
    commands = form.get("commands") or []
    if commands:
        lines.append("\t<Commands>")
        for index, command in enumerate(commands, start=1):
            lines.append(emit_command(command, index))
        lines.append("\t</Commands>")
    lines.append("</Form>")
    return "\n".join(lines)


def emit_form_metadata(form: dict, object_name: str) -> str:
    uuid_text = deterministic_uuid("form", object_name, form["name"])
    synonym = form.get("synonym", form["name"])
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<MetaDataObject %s version="2.20">' % namespace_attrs(),
             '\t<Form uuid="%s">' % uuid_text,
             "\t\t<Properties>",
             "\t\t\t<Name>%s</Name>" % escape(form["name"]),
             "\t\t\t<Synonym>",
             "\t\t\t\t<v8:item>",
             "\t\t\t\t\t<v8:lang>ru</v8:lang>",
             "\t\t\t\t\t<v8:content>%s</v8:content>" % escape(synonym),
             "\t\t\t\t</v8:item>",
             "\t\t\t</Synonym>",
             "\t\t\t<Comment/>",
             "\t\t\t<FormType>Managed</FormType>",
             "\t\t\t<IncludeHelpInContents>false</IncludeHelpInContents>",
             "\t\t\t<UsePurposes>",
             '\t\t\t\t<v8:Value xsi:type="app:ApplicationUsePurpose">'
             "PlatformApplication</v8:Value>",
             "\t\t\t</UsePurposes>",
             "\t\t\t<ExtendedPresentation/>",
             "\t\t</Properties>",
             "\t</Form>",
             "</MetaDataObject>"]
    return "\n".join(lines)


def emit_data_processor(spec: dict) -> str:
    processor = spec["dataProcessor"]
    name = processor["name"]
    uuid_text = deterministic_uuid("dataProcessor", name)
    forms = [form["name"] for form in spec.get("forms", [])]
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<MetaDataObject %s version="2.20">' % namespace_attrs(),
             '\t<DataProcessor uuid="%s">' % uuid_text,
             "\t\t<InternalInfo>",
             '\t\t\t<xr:GeneratedType name="DataProcessorObject.%s" category="Object">'
             % escape(name),
             "\t\t\t\t<xr:TypeId>%s</xr:TypeId>"
             % deterministic_uuid("type", "object", name),
             "\t\t\t\t<xr:ValueId>%s</xr:ValueId>"
             % deterministic_uuid("value", "object", name),
             "\t\t\t</xr:GeneratedType>",
             '\t\t\t<xr:GeneratedType name="DataProcessorManager.%s" category="Manager">'
             % escape(name),
             "\t\t\t\t<xr:TypeId>%s</xr:TypeId>"
             % deterministic_uuid("type", "manager", name),
             "\t\t\t\t<xr:ValueId>%s</xr:ValueId>"
             % deterministic_uuid("value", "manager", name),
             "\t\t\t</xr:GeneratedType>",
             "\t\t</InternalInfo>",
             "\t\t<Properties>",
             "\t\t\t<Name>%s</Name>" % escape(name),
             "\t\t\t<Synonym>",
             "\t\t\t\t<v8:item>",
             "\t\t\t\t\t<v8:lang>ru</v8:lang>",
             "\t\t\t\t\t<v8:content>%s</v8:content>"
             % escape(processor.get("synonym", name)),
             "\t\t\t\t</v8:item>",
             "\t\t\t</Synonym>",
             "\t\t\t<Comment/>",
             "\t\t\t<UseStandardCommands>true</UseStandardCommands>",
             ("\t\t\t<DefaultForm>DataProcessor.%s.Form.%s</DefaultForm>"
              % (escape(name), escape(forms[0]))) if forms else "\t\t\t<DefaultForm/>",
             "\t\t\t<AuxiliaryForm/>",
             "\t\t\t<IncludeHelpInContents>false</IncludeHelpInContents>",
             "\t\t\t<ExtendedPresentation/>",
             "\t\t\t<Explanation/>",
             "\t\t</Properties>",
             "\t\t<ChildObjects>"]
    for form in forms:
        lines.append("\t\t\t<Form>%s</Form>" % escape(form))
    lines += ["\t\t</ChildObjects>", "\t</DataProcessor>", "</MetaDataObject>"]
    return "\n".join(lines)


def emit_configuration(spec: dict) -> str:
    skeleton_file = os.path.join(SKELETON, "Configuration.xml")
    if not os.path.isfile(skeleton_file):
        raise SystemExit("нет скелета конфигурации: " + skeleton_file)
    with open(skeleton_file, encoding="utf-8-sig") as handle:
        text = handle.read()
    configuration = spec["configuration"]
    name = configuration["name"]
    text = _replace_attribute(text, "Configuration", "uuid",
                              deterministic_uuid("configuration", name))
    text = _replace_simple(text, "Name", name)
    text = _replace_simple(text, "Synonym", configuration.get("synonym", name),
                           item=True)
    text = _replace_simple(text, "Version", "0.1")
    child_objects = ["\t\t\t<Language>Русский</Language>"]
    if spec.get("dataProcessor"):
        child_objects.append("\t\t\t<DataProcessor>%s</DataProcessor>"
                             % escape(spec["dataProcessor"]["name"]))
    start = text.index("<ChildObjects>")
    end = text.index("</ChildObjects>") + len("</ChildObjects>")
    text = (text[:start] + "<ChildObjects>\n" + "\n".join(child_objects)
            + "\n\t\t</ChildObjects>" + text[end:])
    return text


def _replace_attribute(text: str, element: str, attribute: str, value: str) -> str:
    pattern = re.compile(r'(<%s\s[^>]*?\b%s=")[^"]*(")' % (element, attribute))
    return pattern.sub(lambda match: match.group(1) + value + match.group(2),
                       text, count=1)


def _replace_simple(text: str, element: str, value: str, item: bool = False) -> str:
    if item:
        replacement = ("<%s><v8:item><v8:lang>ru</v8:lang>"
                       "<v8:content>%s</v8:content></v8:item></%s>"
                       % (element, escape(value), element))
    else:
        replacement = "<%s>%s</%s>" % (element, escape(value), element)
    pattern = re.compile(r"<%s\s*/>|<%s>.*?</%s>" % (element, element, element),
                         re.DOTALL)
    if not pattern.search(text):
        raise SystemExit("в скелете нет элемента " + element)
    return pattern.sub(lambda match: replacement, text, count=1)


def emit_dump_config_info(spec: dict) -> str:
    name = spec["configuration"]["name"]
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<ConfigDumpInfo xmlns="http://v8.1c.ru/8.3/xcf/dumpinfo" '
             'xmlns:xen="http://v8.1c.ru/8.3/xcf/enums" '
             'xmlns:xs="http://www.w3.org/2001/XMLSchema" '
             'xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" '
             'format="Hierarchical" version="2.20">',
             "\t<ConfigVersions>",
             '\t\t<Metadata name="Configuration.%s" id="%s" configVersion="%s"/>'
             % (escape(name), deterministic_uuid("configuration", name),
                "0000000000000000000000000000000000000000"),
             "\t</ConfigVersions>",
             "</ConfigDumpInfo>"]
    return "\n".join(lines)


def command_generate(args) -> None:
    spec = load_spec(args.spec)
    vocab = load_vocab()
    warnings: list = []
    if os.path.isdir(CONFIG_FILES):
        shutil.rmtree(CONFIG_FILES)
    os.makedirs(CONFIG_FILES, exist_ok=True)

    # язык — из скелета без изменений
    language_src = os.path.join(SKELETON, "Languages", "Русский.xml")
    language_dst = os.path.join(CONFIG_FILES, "Languages", "Русский.xml")
    os.makedirs(os.path.dirname(language_dst), exist_ok=True)
    shutil.copyfile(language_src, language_dst)

    write_text(os.path.join(CONFIG_FILES, "Configuration.xml"),
               emit_configuration(spec))
    write_text(os.path.join(CONFIG_FILES, "ConfigDumpInfo.xml"),
               emit_dump_config_info(spec))

    processor = spec.get("dataProcessor")
    if processor:
        name = processor["name"]
        write_text(os.path.join(CONFIG_FILES, "DataProcessors", name + ".xml"),
                   emit_data_processor(spec))
        for form in spec.get("forms", []):
            base = os.path.join(CONFIG_FILES, "DataProcessors", name, "Forms")
            write_text(os.path.join(base, form["name"] + ".xml"),
                       emit_form_metadata(form, name))
            write_text(os.path.join(base, form["name"], "Ext", "Form.xml"),
                       emit_form_xml(form, vocab, warnings))
            module = emit_form_module(form)
            if module:
                write_text(os.path.join(base, form["name"], "Ext", "Form",
                                        "Module.bsl"), module)

    for warning in sorted(set(warnings)):
        print("  предупреждение: " + warning)
    forms = len(spec.get("forms", []))
    elements = sum(len(form.get("elements", [])) for form in spec.get("forms", []))
    print("выгрузка: %s, форм %d, элементов %d" % (CONFIG_FILES, forms, elements))


def command_init(args) -> None:
    executable = platform_for(args)
    base = args.base
    if os.path.isfile(os.path.join(base, "1Cv8.1CD")):
        print("база уже существует: " + base)
        return
    os.makedirs(base, exist_ok=True)
    os.makedirs(LOG_DIR, exist_ok=True)
    log = os.path.join(LOG_DIR, "init.log")
    code = run_batch(executable, ["CREATEINFOBASE", "File=%s;" % base], log)
    if code != 0 or not os.path.isfile(os.path.join(base, "1Cv8.1CD")):
        raise SystemExit("не удалось создать базу: " + base)


def command_push(args) -> None:
    executable = platform_for(args)
    if not os.path.isdir(CONFIG_FILES):
        raise SystemExit("нет выгрузки, сначала generate")
    log = os.path.join(LOG_DIR, "push.log")
    code = run_batch(executable, ["DESIGNER", "/F" + args.base,
                                  "/LoadConfigFromFiles", CONFIG_FILES,
                                  "/UpdateDBCfg"], log)
    text = read_log(log)
    if code != 0 or "Ошибка" in text or "Error" in text:
        raise SystemExit("загрузка не удалась, см. " + log)


def command_pull(args) -> None:
    executable = platform_for(args)
    target = args.out or CONFIG_FILES
    if os.path.isdir(target):
        shutil.rmtree(target)
    log = os.path.join(LOG_DIR, "pull.log")
    code = run_batch(executable, ["DESIGNER", "/F" + args.base,
                                  "/DumpConfigToFiles", target], log)
    if code != 0 or not os.path.isdir(target):
        raise SystemExit("выгрузка из базы не удалась, см. " + log)
    print("выгрузка из базы: " + target)


def command_cf(args) -> None:
    executable = platform_for(args)
    log = os.path.join(LOG_DIR, "cf.log")
    if os.path.isfile(CF_FILE):
        os.remove(CF_FILE)
    code = run_batch(executable, ["DESIGNER", "/F" + args.base,
                                  "/DumpCfg", CF_FILE], log)
    if code != 0 or not os.path.isfile(CF_FILE):
        raise SystemExit("выгрузка .cf не удалась, см. " + log)
    print("контейнер: %s, %d КБ" % (CF_FILE, os.path.getsize(CF_FILE) // 1024))


def command_build(args) -> None:
    command_generate(args)
    command_init(args)
    command_push(args)
    # Платформа при сохранении дописывает служебные дочерние элементы
    # (контекстное меню, расширенная подсказка), упорядочивает свойства и
    # считает хеши: обратный дамп становится фикстурой, а спецификация —
    # только способом её создать.
    command_pull(args)
    command_cf(args)


def command_check(args) -> None:
    """Сверка сгенерированной выгрузки с тем, что платформа сохранила обратно."""
    with tempfile.TemporaryDirectory() as temp:
        executable = platform_for(args)
        log = os.path.join(LOG_DIR, "check.log")
        code = run_batch(executable, ["DESIGNER", "/F" + args.base,
                                      "/DumpConfigToFiles", temp], log)
        if code != 0:
            raise SystemExit("выгрузка для сверки не удалась, см. " + log)
        differences = compare_dumps(CONFIG_FILES, temp)
    if not differences:
        print("сверка: расхождений нет")
        return
    print("сверка: %d расхождений" % len(differences))
    for line in differences[:120]:
        print("  " + line)


def _local(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def _canonical(path: str) -> str:
    tree = ET.parse(path)
    return _element_text(tree.getroot())


def _element_text(element) -> str:
    children = ["%s:%s" % (_local(child.tag), _element_text(child))
                for child in element]
    text = (element.text or "").strip()
    return "%s[%s]{%s}" % (_local(element.tag), text, ",".join(children))


def compare_dumps(expected: str, actual: str) -> list:
    differences = []
    for base, _dirs, files in os.walk(expected):
        for name in files:
            if name == "ConfigDumpInfo.xml":
                continue
            path = os.path.join(base, name)
            relative = os.path.relpath(path, expected)
            other = os.path.join(actual, relative)
            if not os.path.isfile(other):
                differences.append("нет в обратном дампе: " + relative)
                continue
            try:
                if _canonical(path) != _canonical(other):
                    differences.append("расхождение: " + relative)
            except ET.ParseError as error:
                differences.append("не разобрать %s: %s" % (relative, error))
    return differences


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", default=DEFAULT_BASE,
                        help="каталог файловой ИБ (по умолчанию %s)" % DEFAULT_BASE)
    parser.add_argument("--platform", default=None,
                        help="каталог платформы 1С или путь к 1cv8.exe")
    parser.add_argument("--spec", default=SPEC_FILE,
                        help="спецификация форм и контролов")
    parser.add_argument("--out", default=None,
                        help="куда писать выгрузку (для pull)")
    sub = parser.add_subparsers(dest="command", required=True)
    for name, handler in (("generate", command_generate), ("init", command_init),
                          ("push", command_push), ("pull", command_pull),
                          ("cf", command_cf), ("build", command_build),
                          ("check", command_check)):
        sub.add_parser(name).set_defaults(handler=handler)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    args.handler(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
