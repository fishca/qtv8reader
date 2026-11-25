#!/usr/bin/env python3
"""Матрица контролов: что из корпуса покрыто тестовой базой и что с ним в сверке.

Матрица нужна, чтобы не угадывать, какой контрол брать следующим. Она сводит
четыре независимых источника:

* **спецификация** (``spec/controls.json``) — какие теги стоят в тестовых формах;
* **словарь корпуса** (``vocab.json``) — в скольких формах реальных конфигураций
  тег встречается (это и есть приоритет);
* **модель** — дерево элементов, которое ``v8reader_cli form-model`` собирает из
  фикстурного контейнера (если тег есть в спецификации, но пропал в модели,
  ячейка это покажет);
* **сверка** (``out/control/<форма>/result.json``) — структурные и текстовые
  расхождения с эталоном конфигуратора, если эталон уже снят.

Отдельным столбцом идёт поддержка рендерера: он объявляет свои теги в
``FORM_TAGS``/``RARE_TAGS`` (``resources/formpreview/js/form-preview.js``),
поэтому видно, какие контролы база уже может показать, а какие только предстоит
разобрать в модели.

Запуск::

    python tools/control_matrix.py                      # таблица в консоль
    python tools/control_matrix.py --write              # + docs/controls-matrix.md
    python tools/control_matrix.py --cli out/mingw/v8reader_cli.exe
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURE = os.path.join(REPO, "tests", "fixtures", "controlbase")
SPEC = os.path.join(FIXTURE, "spec", "controls.json")
VOCAB = os.path.join(FIXTURE, "vocab.json")
REFS = os.path.join(FIXTURE, "refs", "manifest.json")
CF = os.path.join(FIXTURE, "1Cv8_controls.cf")
MODELS = os.path.join(REPO, "out", "control", "matrix")
RESULTS = os.path.join(REPO, "out", "control")
RENDERER = os.path.join(REPO, "resources", "formpreview", "js", "form-preview.js")
DOC = os.path.join(REPO, "docs", "controls-matrix.md")


# --------------------------------------------------------------------------
# Источники


def load_json(path: str, default=None):
    if not os.path.isfile(path):
        return default
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def spec_tags(form: dict) -> list:
    """Теги элементов формы в порядке появления (вложенность разворачивается)."""
    tags = []

    def walk(elements):
        for element in elements or []:
            tags.append(element.get("tag", ""))
            walk(element.get("children"))

    walk(form.get("elements"))
    return tags


def spec_elements(form: dict) -> list:
    """Плоский список (тег, имя) всех элементов формы."""
    found = []

    def walk(elements):
        for element in elements or []:
            found.append((element.get("tag", ""), element.get("name", "")))
            walk(element.get("children"))

    walk(form.get("elements"))
    return found


def model_tags(cli: str, form: str) -> list | None:
    """Теги из JSON-модели фикстурного контейнера (None — модель не собрана)."""
    if not cli or not os.path.isfile(cli) or not os.path.isfile(CF):
        return None
    os.makedirs(MODELS, exist_ok=True)
    target = os.path.join(MODELS, form + ".json")
    if not os.path.isfile(target):
        result = subprocess.run(
            [cli, "form-model", CF, "Обработка.Контролы", "--form", form,
             "-o", target],
            capture_output=True, text=True, encoding="utf-8", errors="replace")
        if result.returncode != 0:
            target = ""
    if not target or not os.path.isfile(target):
        return None
    model = load_json(target, {})
    tags = []

    def walk(items):
        for item in items or []:
            tags.append(item.get("tag", ""))
            for key in ("childItems",):
                walk(item.get(key))
            if item.get("autoCommandBar"):
                walk([item["autoCommandBar"]])

    walk(model.get("childItemsRoot"))
    return tags


def renderer_tags() -> dict:
    """Теги, которые рендерер объявляет: FORM_TAGS (обычные) и RARE_TAGS.

    Тег из ``RARE_TAGS`` рендерер тоже знает, но показывает упрощённо (полотно
    документа, календарь-заглушка), поэтому в матрице он помечен отдельно.
    """
    supported = {}
    if not os.path.isfile(RENDERER):
        return supported
    with open(RENDERER, encoding="utf-8") as handle:
        text = handle.read()
    for block, mark in (("RARE_TAGS", "частично"), ("FORM_TAGS", "да")):
        match = re.search(r"var %s = \{(.*?)\};" % block, text, re.S)
        if not match:
            continue
        for name in re.findall(r"(\w+)\s*:", match.group(1)):
            if supported.get(name) != "да":
                supported[name] = mark
    return supported


def compare_results() -> dict:
    """Итоги сверки по формам: структура, текст, полотно."""
    results = {}
    for path in sorted(glob.glob(os.path.join(RESULTS, "*", "result.json"))):
        data = load_json(path, {})
        form = data.get("form")
        if not form:
            continue
        results[form] = data
    return results


# --------------------------------------------------------------------------
# Матрица


def build(cli: str) -> dict:
    spec = load_json(SPEC, {})
    vocab = load_json(VOCAB, {}) or {}
    corpus = {tag: int(entry.get("count", 0))
              for tag, entry in (vocab.get("tags") or {}).items()}
    refs = (load_json(REFS, {}) or {}).get("forms", {})
    renderer = renderer_tags()
    results = compare_results()

    forms = []
    for form in spec.get("forms", []):
        name = form.get("name", "")
        authored = spec_tags(form)
        modeled = model_tags(cli, name)
        forms.append({
            "name": name,
            "synonym": form.get("synonym", ""),
            "attributes": len(form.get("attributes") or []),
            "authored": authored,
            "authored_set": set(authored),
            "modeled": modeled,
            "modeled_set": set(modeled or []),
            "ref": refs.get(name),
            "result": results.get(name),
        })

    used = set()
    for form in forms:
        used |= form["authored_set"]

    rows = []
    for tag in corpus:
        covered = [form for form in forms if tag in form["authored_set"]]
        rows.append({
            "tag": tag,
            "corpus": corpus[tag],
            "forms": covered,
            "renderer": renderer.get(tag),
            "modeled": all(tag in form["modeled_set"] for form in covered)
            if covered else None,
        })
    rows.sort(key=lambda row: (-row["corpus"], row["tag"]))
    missing = [row for row in rows if not row["forms"]]
    return {"forms": forms, "rows": rows, "missing": missing,
            "corpus_tags": set(corpus), "used": used}


def coverage_cell(tag: str, form: dict) -> str:
    if tag not in form["authored_set"]:
        return "—"
    if form["modeled"] is not None and tag not in form["modeled_set"]:
        return "нет"
    return "есть"


def form_status(form: dict) -> str:
    if not form["ref"]:
        return "нет эталона"
    result = form["result"]
    if not result:
        return "не сверена"
    if result["structure"] == 0:
        return "ок"
    return "расхождение"


def print_summary(data: dict) -> None:
    rows, forms = data["rows"], data["forms"]
    print("форм в базе: %d, тегов в корпусе: %d, покрыто: %d"
          % (len(forms), len(rows), len(rows) - len(data["missing"])))
    print()
    print("%-24s %7s %5s %8s" % ("тег", "корпус", "база", "рендерер"))
    for row in rows:
        if not row["forms"] and not row["renderer"]:
            continue
        print("%-24s %7d %5d %8s"
              % (row["tag"], row["corpus"], len(row["forms"]),
                 row["renderer"] or "нет"))
    print()
    print("%-16s %5s %10s %10s %8s %s"
          % ("форма", "элем", "эталон", "структура", "текст", "статус"))
    for form in forms:
        result = form["result"] or {}
        print("%-16s %5d %10s %10s %8s %s"
              % (form["name"], len(form["authored"]),
                 ("%dx%d" % (form["ref"]["width"], form["ref"]["height"]))
                 if form["ref"] else "—",
                 result.get("structure", "—"), result.get("text", "—"),
                 form_status(form)))


def write_doc(data: dict, path: str = DOC) -> None:
    rows, forms = data["rows"], data["forms"]
    covered = len(rows) - len(data["missing"])
    lines = [
        "# Матрица контролов",
        "",
        "Таблица собирается из спецификации тестовой базы, словаря корпуса,",
        "JSON-модели формы и итогов пиксельной сверки:",
        "",
        "```powershell",
        "python tools/control_matrix.py --write",
        "```",
        "",
        "Столбец **Корпус** — в скольких формах корпуса (выгрузка БП) встречается",
        "тег; это приоритет работы. **База** — сколько тестовых форм его содержат.",
        "**Модель** — доходит ли тег до JSON-модели (иначе элемент теряется при",
        "разборе контейнера). **Рендерер** — объявлен ли тег в `FORM_TAGS`",
        "`resources/formpreview/js/form-preview.js`.",
        "",
        "Покрыто тегов: **%d из %d**." % (covered, len(rows)),
        "",
        "## Теги",
        "",
        "| Тег | Корпус | База | Формы базы | Модель | Рендерер |",
        "|---|---|---|---|---|---|",
    ]
    for row in rows:
        if not row["forms"] and not row["renderer"]:
            continue
        names = ", ".join(form["name"] for form in row["forms"]) or "—"
        model = "—" if row["modeled"] is None else ("есть" if row["modeled"] else "нет")
        lines.append("| `%s` | %d | %d | %s | %s | %s |"
                     % (row["tag"], row["corpus"], len(row["forms"]), names,
                        model, row["renderer"] or "нет"))
    lines += ["", "## Формы базы", "",
              "| Форма | Элементов | Эталон | Структура | Текст | Статус |",
              "|---|---|---|---|---|---|"]
    for form in forms:
        result = form["result"] or {}
        ref = ("%dx%d" % (form["ref"]["width"], form["ref"]["height"])
               if form["ref"] else "—")
        lines.append("| %s | %d | %s | %s | %s | %s |"
                     % (form["name"], len(form["authored"]), ref,
                        result.get("structure", "—"), result.get("text", "—"),
                        form_status(form)))
    lines += ["", "## Очередь", "",
              "Теги корпуса, которых ещё нет в базе. Порядок — по частоте в",
              "корпусе: сверху то, что даст больше всего формам сразу.", "",
              "| Тег | Корпус | Рендерер |", "|---|---|---|"]
    for row in data["missing"]:
        lines.append("| `%s` | %d | %s |"
                     % (row["tag"], row["corpus"], row["renderer"] or "нет"))
    lines += ["", "## Как пополнять", "",
              "1. Дописать форму или элемент в `spec/controls.json`.",
              "2. `python tools/control_base.py build`.",
              "3. `python tools/control_refs.py capture --all`.",
              "4. `python tools/control_compare.py --all --update-baseline`.",
              "5. `python tools/control_matrix.py --write` — обновить эту таблицу.",
              ""]
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines))
    print("матрица: " + os.path.relpath(path, REPO))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--cli", default=os.path.join(REPO, "out", "mingw",
                                                      "v8reader_cli.exe"),
                        help="путь к v8reader_cli (нужен для столбца «Модель»)")
    parser.add_argument("--write", action="store_true",
                        help="обновить docs/controls-matrix.md")
    parser.add_argument("--refresh", action="store_true",
                        help="пересобрать модели форм заново")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.refresh and os.path.isdir(MODELS):
        for path in glob.glob(os.path.join(MODELS, "*.json")):
            os.remove(path)
    data = build(args.cli)
    print_summary(data)
    if args.write:
        write_doc(data)
    return 0


if __name__ == "__main__":
    sys.exit(main())
