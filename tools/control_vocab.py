#!/usr/bin/env python3
"""Словарь элементов и свойств управляемых форм по выгрузке конфигурации.

Платформа пишет ``Ext/Form.xml`` в порядке XSD-схемы, поэтому по корпусу
выгрузок можно восстановить не только набор свойств каждого элемента, но и
**порядок**, в котором их требуется писать: генератор тестовых форм
(``tools/control_base.py``) без этого не пройдёт загрузку в 1С.

Сканер проходит по всем ``*Forms/*/Ext/Form.xml`` каталога выгрузки, для
каждого элемента формы запоминает последовательность его непосредственных
дочерних свойств и сводит наблюдения в частичный порядок (a раньше b),
который затем топологически сортируется.

    python tools/control_vocab.py --dump E:/work/dt_cf/bp/ConfigFiles \\
        -o tests/fixtures/controlbase/vocab.json
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import sys
import xml.etree.ElementTree as ET

LOG_FORM = "http://v8.1c.ru/8.3/xcf/logform"
NAMESPACES = {"lf": LOG_FORM, "v8": "http://v8.1c.ru/8.1/data/core"}


def local(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def dump_roots(root: str):
    for base, _dirs, files in os.walk(root):
        for name in files:
            if name != "Form.xml":
                continue
            path = os.path.join(base, name)
            # берём только формы: путь .../Forms/<форма>/Ext/Form.xml
            parts = path.replace("\\", "/").split("/")
            if "Forms" not in parts:
                continue
            yield path


def scan(root: str, limit: int | None) -> dict:
    # Наблюдения: для каждой пары свойств одного элемента — сколько раз
    # «a» шло раньше «b». Из этих счётчиков восстанавливается порядок схемы.
    before: dict[str, dict[tuple, int]] = collections.defaultdict(
        lambda: collections.defaultdict(int))
    seen: dict[str, int] = collections.Counter()
    properties: dict[str, set] = collections.defaultdict(set)
    form_before: dict[tuple, int] = collections.defaultdict(int)
    forms = 0

    for index, path in enumerate(dump_roots(root)):
        if limit is not None and index >= limit:
            break
        try:
            tree = ET.parse(path)
        except ET.ParseError as error:
            print("пропуск %s: %s" % (path, error), file=sys.stderr)
            continue
        forms += 1
        form = tree.getroot()
        if local(form.tag) != "Form":
            continue
        sequence = [local(child.tag) for child in form
                    if local(child.tag) != "ChildItems"]
        _add_pairs(form_before, sequence)

        for parent in form.iter():
            if local(parent.tag) != "ChildItems":
                continue
            for item in parent:
                tag = local(item.tag)
                seen[tag] += 1
                sequence = [local(child.tag) for child in item]
                properties[tag].update(sequence)
                _add_pairs(before[tag], sequence)

    tags = {}
    for tag, count in sorted(seen.items()):
        order = _order_from_pairs(properties[tag], before[tag])
        tags[tag] = {
            "count": count,
            "order": order,
            "properties": sorted(properties[tag]),
        }
    return {
        "forms": forms,
        "form_order": _order_from_pairs(set(form_before), form_before),
        "tags": tags,
    }


def _add_pairs(counter: dict, sequence: list) -> None:
    for i, first in enumerate(sequence):
        for second in sequence[i + 1:]:
            counter[(first, second)] += 1


def _order_from_pairs(nodes: set, pairs: dict[tuple, int]) -> list:
    """Порядок по большинству: «a раньше b» из пар свойств одного элемента."""
    import functools

    nodes = set(nodes)

    def compare(left, right):
        forward = pairs.get((left, right), 0)
        backward = pairs.get((right, left), 0)
        if forward != backward:
            return -1 if forward > backward else 1
        return -1 if left < right else (1 if left > right else 0)

    return sorted(nodes, key=functools.cmp_to_key(compare))


def _toposort(nodes: set, edges: dict[str, set]) -> list:
    """Топологическая сортировка с устойчивым порядком при равенстве."""
    nodes = set(nodes)
    edges = {a: {b for b in (edges.get(a) or set()) if b in nodes}
             for a in nodes}
    # убираем транзитивные циклы (платформа иногда пишет свойства в разных
    # порядках), оставляя первые встреченные ограничения
    order: list = []
    while nodes:
        free = [n for n in nodes if not any(n in edges[o] for o in nodes
                                            if o != n)]
        if not free:
            # цикл: разрываем по имени, чтобы результат был детерминирован
            free = [min(nodes)]
        free.sort()
        for node in free:
            order.append(node)
            nodes.discard(node)
    return order


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dump", default=r"E:\work\dt_cf\bp\ConfigFiles",
                        help="каталог выгрузки конфигурации в файлы")
    parser.add_argument("--limit", type=int, default=None,
                        help="ограничить число форм (для проб)")
    parser.add_argument("-o", "--out",
                        default=os.path.join(os.path.dirname(os.path.dirname(
                            os.path.abspath(__file__))),
                            "tests", "fixtures", "controlbase", "vocab.json"))
    args = parser.parse_args()

    if not os.path.isdir(args.dump):
        raise SystemExit("нет каталога выгрузки: " + args.dump)
    result = scan(args.dump, args.limit)
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=1)
        handle.write("\n")
    print("форм: %d, тегов: %d → %s" % (result["forms"], len(result["tags"]),
                                        args.out))
    for tag in ("Form", "InputField", "UsualGroup", "Table", "Button"):
        entry = result["tags"].get(tag)
        if entry:
            print("  %-14s %6d  %s" % (tag, entry["count"],
                                       ", ".join(entry["order"][:12])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
