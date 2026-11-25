#!/usr/bin/env python3
"""Снимок геометрии DOM управляемой формы без картинки.

Пиксельная сверка отвечает на вопрос «где разошлось», но не на вопрос
«почему»: по растеризованной рамке не видно, какой узел получил ширину,
какой класс растягивания ему выставили и какую ширину ему дал родитель.
Скрипт рендерит ту же страницу, что и ``form_view_measure.py`` (iframe
ровно в размере эталона), и вытаскивает из живого DOM прямоугольники
узлов вместе с их раскладкой.

Запуск::

    python tools/form_view_probe.py --model out/tmp/groups.json \\
        --width 384 --height 173

Печатает JSON-массив узлов: тег, классы, прямоугольник и атрибуты
``data-fp-*``, по которым рендерер решает про ширину.
"""

from __future__ import annotations

import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))
import form_view_measure as measure  # noqa: E402

PAGE_ROOT = os.path.join(REPO, "out", "preview")
PROBE_DIR = os.path.join(PAGE_ROOT, "probe")
FRAME = os.path.join(PAGE_ROOT, "_probe_geo.html")

DEFAULT_SNIPPET = r"""
(function () {
    var list = [];
    var nodes = document.querySelectorAll(SELECTOR);
    for (var i = 0; i < nodes.length; i++) {
        var el = nodes[i];
        var r = el.getBoundingClientRect();
        var row = {
            tag: el.getAttribute('data-tag') || '',
            cls: String(el.className || '').slice(0, 200),
            x: Math.round(r.left * 10) / 10,
            y: Math.round(r.top * 10) / 10,
            w: Math.round(r.width * 10) / 10,
            h: Math.round(r.height * 10) / 10
        };
        ['fpAuthoredWidthPresent', 'fpWidthStretch', 'fpAuthoredNormalWidth',
         'fpDeclaredWidthBand', 'fpAuthoredDecisionWidth'].forEach(function (key) {
            if (el.dataset && el.dataset[key] != null) row[key] = el.dataset[key];
        });
        if (el.getAttribute('data-name')) row.name = el.getAttribute('data-name');
        if (el.getAttribute('data-field-kind')) row.kind = el.getAttribute('data-field-kind');
        list.push(row);
    }
    return list;
})()
"""

FRAME_TEMPLATE = """<!doctype html>
<html lang="ru"><head><meta charset="utf-8">
<style>html,body{margin:0;padding:0;overflow:hidden;background:#fff}
iframe{border:0;display:block;width:__W__px;height:__H__px}</style></head>
<body><iframe id="target" src="model.html?t=__STAMP__&amp;model=probe/model.json"></iframe>
<pre id="probe-result">PENDING</pre>
<script>
(function () {
    var frame = document.getElementById('target');
    var result = document.getElementById('probe-result');
    var SELECTOR = __SELECTOR__;
    var SNIPPET = __SNIPPET__;
    function emit(value) {
        try {
            result.textContent = 'PROBE_BEGIN' + JSON.stringify(value) + 'PROBE_END';
        } catch (error) {
            result.textContent = 'PROBE_BEGIN' + JSON.stringify({ error: String(error) }) + 'PROBE_END';
        }
    }
    function collect() {
        var win = frame.contentWindow;
        var doc = frame.contentDocument;
        if (!win || !doc || !win.__QV8_READY) return false;
        /* The parentheses keep automatic semicolon insertion from ending the
         * statement right after `return`: a snippet starting on a new line
         * would otherwise evaluate to undefined. */
        var fn = new win.Function('SELECTOR', 'return (' + SNIPPET + ');');
        emit(fn.call(doc, SELECTOR));
        return true;
    }
    var tries = 0;
    frame.addEventListener('load', function () {
        var timer = setInterval(function () {
            tries++;
            var done = false;
            try {
                done = collect();
            } catch (error) {
                clearInterval(timer);
                emit({ error: String(error && error.stack || error) });
                return;
            }
            if (done || tries > 200) clearInterval(timer);
        }, 50);
    });
})();
</script></body></html>
"""


def prepare(model_path: str) -> None:
    measure.sync_resources(PAGE_ROOT)
    os.makedirs(PROBE_DIR, exist_ok=True)
    shutil.copy2(os.path.abspath(model_path), os.path.join(PROBE_DIR, "model.json"))


def write_frame(width: int, height: int, selector: str, snippet: str) -> None:
    body = FRAME_TEMPLATE
    for key, value in (("__W__", str(width)), ("__H__", str(height)),
                       ("__STAMP__", str(int(time.time())))):
        body = body.replace(key, value)
    body = body.replace("__SELECTOR__", json.dumps(selector))
    body = body.replace("__SNIPPET__", json.dumps(snippet))
    with open(FRAME, "w", encoding="utf-8") as handle:
        handle.write(body)


def dump_dom(url: str) -> str:
    profile = tempfile.mkdtemp(prefix="qv8-probe-")
    try:
        result = subprocess.run(
            [measure.find_chrome(), "--headless=new", "--disable-gpu",
             "--force-device-scale-factor=1", "--virtual-time-budget=10000",
             "--user-data-dir=" + profile, "--window-size=1400,1000",
             "--dump-dom", url],
            capture_output=True, env=measure.qt_env(), timeout=180, check=False,
        )
        return result.stdout.decode("utf-8", "replace")
    finally:
        shutil.rmtree(profile, ignore_errors=True)


def extract(dom: str):
    match = re.search(r"PROBE_BEGIN(.*?)PROBE_END", dom, re.S)
    if not match:
        raise SystemExit("проба не вернула результат:\n" + dom[-1500:])
    return json.loads(html.unescape(match.group(1)))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--model", required=True)
    parser.add_argument("--width", type=int, required=True)
    parser.add_argument("--height", type=int, required=True)
    parser.add_argument("--url", default="http://127.0.0.1:8765")
    parser.add_argument("--selector", default=".fp-item")
    parser.add_argument("--script", default=None,
                        help="файл с JS-выражением; внутри доступен SELECTOR")
    parser.add_argument("--out", default=None)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    snippet = DEFAULT_SNIPPET
    if args.script:
        with open(args.script, encoding="utf-8") as handle:
            snippet = handle.read()
    prepare(args.model)
    write_frame(args.width, args.height, args.selector, snippet)
    url = "%s/_probe_geo.html?t=%d" % (args.url.rstrip("/"), int(time.time()))
    data = extract(dump_dom(url))
    text = json.dumps(data, ensure_ascii=False, indent=1)
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as handle:
            handle.write(text + "\n")
        print("%s: %d" % (args.out, len(data) if isinstance(data, list) else 1))
    else:
        print(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
