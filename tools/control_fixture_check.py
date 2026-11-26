#!/usr/bin/env python3
"""Гейт модели формы: JSON-модель должна совпадать с Form.xml фикстуры.

Проверка не требует 1С и рендерера: для каждой формы тестовой выгрузки
запускается ``v8reader_cli form-model`` на фикстурном ``.cf``, и дерево
элементов сравнивается с ``Ext/Form.xml`` — теги, имена, порядок и значения
свойств. Служебные дочерние элементы, которые платформа дописывает сама
(контекстное меню, расширенная подсказка, события), пропускаются.

    python tools/control_fixture_check.py \\
        --cli out/mingw/v8reader_cli.exe \\
        --dir tests/fixtures/controlbase
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

from console_utf8 import use_utf8_stdio

SKIP_TAGS = {"ContextMenu", "ExtendedTooltip", "Events"}


def local(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def xml_properties(element) -> dict:
    """Свойства элемента формы: простые значения и тексты (Title и т.п.)."""
    result = {}
    for child in element:
        tag = local(child.tag)
        if tag in SKIP_TAGS or tag == "ChildItems":
            continue
        if tag in ("Title", "ToolTip", "ExtendedPresentation", "Explanation"):
            item = child.find("{http://v8.1c.ru/8.1/data/core}item")
            if item is not None:
                content = item.find("{http://v8.1c.ru/8.1/data/core}content")
                result[tag] = (content.text or "") if content is not None else ""
            continue
        if len(child) == 0:
            text = (child.text or "").strip()
            if text:
                result[tag] = text
    # Платформа пишет кнопке <CommandName>0</CommandName>, когда команды нет:
    # ноль — её внутренний «пустой» указатель, в модели такой кнопке нечего
    # отдавать (ресурс команды равен нулю, а не имя команды «0»).
    if result.get("CommandName") == "0":
        del result["CommandName"]
    return result


def xml_tree(element) -> list:
    nodes = []
    for child in element:
        tag = local(child.tag)
        if tag in SKIP_TAGS:
            continue
        if tag == "ChildItems":
            nodes.append(("ChildItems", "", xml_tree(child)))
            continue
        nodes.append((tag, child.get("name", ""), []))
        # вложенность — через отдельный проход ниже
    result = []
    for child in element:
        tag = local(child.tag)
        if tag in SKIP_TAGS:
            continue
        children = []
        for inner in child:
            if local(inner.tag) == "ChildItems":
                children = xml_tree(inner)
        result.append({"tag": tag, "name": child.get("name", ""),
                       "props": xml_properties(child), "children": children})
    return result


def model_tree(nodes: list) -> list:
    result = []
    for node in nodes or []:
        result.append({"tag": node.get("tag", ""), "name": node.get("name", ""),
                       "props": {k: str(v) for k, v in
                                 (node.get("properties") or {}).items()},
                       "children": model_tree(node.get("childItems"))})
    return result


def compare(path: str, expected: list, actual: list, out: list) -> None:
    if len(expected) != len(actual):
        out.append("%s: элементов %d, а в модели %d"
                   % (path, len(expected), len(actual)))
        return
    for index, (left, right) in enumerate(zip(expected, actual)):
        where = "%s/%s[%d]" % (path, left["tag"], index)
        if left["tag"] != right["tag"]:
            out.append("%s: тег %s, в модели %s" % (where, left["tag"], right["tag"]))
            continue
        if left["name"] and left["name"] != right["name"]:
            out.append("%s: имя %s, в модели %s" % (where, left["name"], right["name"]))
        for key, value in left["props"].items():
            model_value = right["props"].get(key)
            if model_value is None:
                out.append("%s: свойство %s не доходит до модели" % (where, key))
            elif model_value != value:
                out.append("%s: %s = %r, в модели %r"
                           % (where, key, value, model_value))
        compare(where, left["children"], right["children"], out)


def find_forms(root: str) -> list:
    pattern = os.path.join(root, "ConfigFiles", "DataProcessors", "*", "Forms",
                           "*", "Ext", "Form.xml")
    return sorted(glob.glob(pattern))


def cli_env() -> dict:
    """PATH с библиотеками Qt: без него v8reader_cli не стартует.

    Запуск из обычной консоли наследует PATH с Qt, а сборочный агент — нет:
    процесс падал с пустым выводом и гейт сообщал «form-model: » на каждой
    форме, ничего не проверив.
    """
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import form_view_measure  # noqa: PLC0415

    return form_view_measure.qt_env()


def main() -> int:
    use_utf8_stdio()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cli", required=True, help="путь к v8reader_cli")
    parser.add_argument("--dir", default="tests/fixtures/controlbase",
                        help="каталог фикстуры")
    args = parser.parse_args()
    cf = os.path.join(args.dir, "1Cv8_controls.cf")
    forms = find_forms(args.dir)
    if not forms:
        raise SystemExit("в фикстуре нет форм: " + args.dir)
    if not os.path.isfile(cf):
        raise SystemExit("нет контейнера: " + cf)
    problems = []
    for form_xml in forms:
        relative = os.path.relpath(form_xml, args.dir)
        parts = relative.split(os.sep)
        form = parts[-3]
        processor = parts[2]
        with open(form_xml, encoding="utf-8-sig") as handle:
            root = ET.fromstring(handle.read())
        child_items = None
        for child in root:
            if local(child.tag) == 'ChildItems':
                child_items = child
                break
        expected = xml_tree(child_items) if child_items is not None else []
        result = subprocess.run(
            [args.cli, "form-model", cf, "Обработка." + processor,
             "--form", form, "-o", os.path.join(
                 os.path.dirname(cf), ".model-" + form + ".json")],
            capture_output=True, text=True, encoding="utf-8", errors="replace",
            env=cli_env())
        if result.returncode != 0:
            problems.append("%s: form-model: %s"
                            % (form, (result.stdout + result.stderr).strip()))
            continue
        with open(os.path.join(os.path.dirname(cf), ".model-" + form + ".json"),
                  encoding="utf-8") as handle:
            model = json.load(handle)
        compare(form, expected, model_tree(model.get("childItemsRoot")), problems)
        os.remove(os.path.join(os.path.dirname(cf), ".model-" + form + ".json"))
    if problems:
        print("расхождений модели с Form.xml: %d" % len(problems))
        for problem in problems[:60]:
            print("  " + problem)
        return 1
    print("форм проверено: %d, расхождений модели с Form.xml нет" % len(forms))
    return 0


if __name__ == "__main__":
    sys.exit(main())
