#!/usr/bin/env python3
"""Сверка вида управляемой формы с конструктором 1С.

Скрипт делает три вещи, которые нужны для попиксельной сверки:

* ``render`` — собирает JSON-модель формы командой ``v8reader_cli form-model``
  и снимает её в HTML-рендерере (headless Chrome) при заданном размере окна;
* ``marks`` — печатает вертикальные границы (рамки полей, разделители колонок
  таблицы) на указанных строках снимка: значения по x сравниваются глазами или
  скриптом с эталоном;
* ``pair`` — склеивает эталон и наш снимок в одну картинку для просмотра.

Эталон снимается с формы, открытой в конфигураторе: вкладка «Форма» рисует её
в масштабе 1:1, полотно обводится синей рамкой, и размер рамки — это размер
снимка (для формы документа «АвансовыйОтчет» — 988x438).

Запуск (Windows, нужен Pillow и Chrome; сервер из ``serve`` раздаёт каталог
с ``model.html`` и ``model.json``)::

    python tools/form_view_measure.py serve --root out/preview --port 8765
    python tools/form_view_measure.py render --out out/ours.png
    python tools/form_view_measure.py marks --image out/ours.png --rows 67,248
    python tools/form_view_measure.py pair --ours out/ours.png --out out/pair.png

По умолчанию объект — ``Документ.АвансовыйОтчет``, форма — ``ФормаДокумента``,
контейнер — ``E:\\work\\dt_cf\\1Cv8_bp.cf``; всё это перекрывается аргументами.
"""

from __future__ import annotations

import argparse
import http.server
import os
import shutil
import socketserver
import subprocess
import sys
import time
import urllib.request

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(REPO, "out", "mingw", "v8reader_cli.exe")
FORM_RESOURCES = os.path.join(REPO, "resources", "formpreview")
DEFAULT_CF = r"E:\work\dt_cf\1Cv8_bp.cf"
DEFAULT_REF = os.path.join(REPO, "out", "ref-ao.png")
CHROME_CANDIDATES = (
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    r"C:\Program Files (x86)\Google\Chrome\Application\chrome.exe",
)


def find_chrome() -> str:
    for path in CHROME_CANDIDATES:
        if os.path.exists(path):
            return path
    raise SystemExit("chrome.exe не найден")


def qt_env() -> dict:
    env = dict(os.environ)
    for prefix in (r"E:\Qt_6\6.11.2\mingw_64\bin", r"C:\Qt\6.9.1\mingw_64\bin",
                   r"E:\msys64\mingw64\bin"):
        if os.path.isdir(prefix):
            env["PATH"] = prefix + ";" + env.get("PATH", "")
            break
    return env


def model_command(args) -> list:
    return [CLI, "form-model", args.cf, args.object, "--form", args.form,
            "-o", os.path.join(args.root, "model.json")]


def sync_resources(root: str) -> int:
    """Копирует ресурсы рендерера в каталог раздачи.

    Снимок идёт по файлам из ``--root``, а правки делаются в
    ``resources/formpreview``. Без этой копии можно смотреть на старый CSS или
    старый скрипт и «не видеть» только что сделанное исправление.
    """
    if not os.path.isdir(FORM_RESOURCES):
        return 0
    copied = 0
    for name in ("model.html", "LICENSE-BslEdit-MIT.txt"):
        source = os.path.join(FORM_RESOURCES, name)
        if os.path.isfile(source):
            shutil.copy2(source, os.path.join(root, name))
            copied += 1
    for folder, suffixes in (("css", (".css",)), ("js", (".js",)), ("std-pictures", (".png", ".svg"))):
        source_dir = os.path.join(FORM_RESOURCES, folder)
        if not os.path.isdir(source_dir):
            continue
        target_dir = os.path.join(root, folder)
        os.makedirs(target_dir, exist_ok=True)
        for name in sorted(os.listdir(source_dir)):
            source = os.path.join(source_dir, name)
            if os.path.isfile(source) and name.lower().endswith(suffixes):
                shutil.copy2(source, os.path.join(target_dir, name))
                copied += 1
    for name in sorted(os.listdir(FORM_RESOURCES)):
        if name.startswith("platform-") and name.lower().endswith(".png"):
            shutil.copy2(os.path.join(FORM_RESOURCES, name), os.path.join(root, name))
            copied += 1
    return copied


def shoot(args, out_path: str) -> None:
    stamp = str(int(time.time()))
    if os.path.exists(out_path):
        os.remove(out_path)
    subprocess.run(
        [find_chrome(), "--headless=new", "--disable-gpu", "--hide-scrollbars",
         "--force-device-scale-factor=1",
         "--window-size=%d,%d" % (args.width, args.height),
         "--screenshot=" + out_path,
         "%s/model.html?t=%s" % (args.url, stamp)],
        capture_output=True, env=qt_env(), timeout=180, check=False,
    )
    if not os.path.exists(out_path):
        raise SystemExit("снимок не создан: " + out_path)


def command_render(args) -> None:
    if not os.path.exists(CLI):
        raise SystemExit("сначала соберите проект: " + CLI)
    out_path = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    os.makedirs(args.root, exist_ok=True)
    sync_resources(args.root)
    result = subprocess.run(model_command(args), capture_output=True, text=True,
                            encoding="utf-8", errors="replace", env=qt_env())
    if result.returncode != 0:
        raise SystemExit("form-model: " + (result.stdout + result.stderr).strip())
    try:
        urllib.request.urlopen(args.url + "/model.json", timeout=5).read()
    except Exception as error:  # noqa: BLE001
        raise SystemExit("сервер не отвечает (%s): %s" % (args.url, error))
    shoot(args, out_path)
    size = image_size(out_path)
    print("%s %dx%d" % (os.path.basename(out_path), size[0], size[1]))


def image_size(path: str) -> tuple:
    from PIL import Image

    return Image.open(path).size


def marks_of(path: str, rows: list) -> dict:
    """Вертикальные линии рамок: непрерывные участки серого цвета по строке."""
    import numpy as np
    from PIL import Image

    image = np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)
    result = {}
    for y in rows:
        row = image[y]
        maximum = row.max(axis=1)
        minimum = row.min(axis=1)
        mask = (maximum - minimum <= 22) & (minimum >= 140) & (maximum <= 240)
        runs = []
        for x in np.nonzero(mask)[0]:
            x = int(x)
            if runs and x - runs[-1][-1] <= 1:
                runs[-1].append(x)
            else:
                runs.append([x])
        result[y] = [(run[0], run[-1]) for run in runs]
    return result


def command_marks(args) -> None:
    rows = [int(value) for value in args.rows.split(",") if value.strip()]
    paths = [args.image] + ([args.ref] if args.ref else [])
    for path in paths:
        for y, runs in marks_of(path, rows).items():
            text = " ".join("%d" % a if a == b else "%d-%d" % (a, b) for a, b in runs)
            print("%-22s y=%-4d %s" % (os.path.basename(path), y, text))


def command_pair(args) -> None:
    from PIL import Image

    width = args.width
    top, bottom = args.top, args.bottom
    ref = Image.open(args.ref).convert("RGB").crop((0, top, width, bottom))
    ours = Image.open(args.ours).convert("RGB").crop((0, top, width, bottom))
    height = ref.size[1]
    canvas = Image.new("RGB", (width, height * 2 + 14), (255, 0, 0))
    canvas.paste(ref, (0, 0))
    canvas.paste(ours, (0, height + 14))
    canvas.save(args.out)
    print("%s %dx%d" % (os.path.basename(args.out), canvas.size[0], canvas.size[1]))


def command_sync(args) -> None:
    count = sync_resources(os.path.abspath(args.root))
    print("разложено файлов: %d" % count)


def command_serve(args) -> None:
    root = os.path.abspath(args.root)

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *handler_args, **handler_kwargs):
            super().__init__(*handler_args, directory=root, **handler_kwargs)

        def end_headers(self):
            self.send_header("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0")
            self.send_header("Pragma", "no-cache")
            super().end_headers()

        def log_message(self, *handler_args):
            pass

    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", args.port), Handler) as server:
        print("раздаю %s на http://127.0.0.1:%d" % (root, args.port))
        server.serve_forever()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    def add_render_options(target):
        target.add_argument("--cf", default=DEFAULT_CF, help="контейнер .cf/.epf")
        target.add_argument("--object", default="Документ.АвансовыйОтчет",
                            help="объект метаданных")
        target.add_argument("--form", default="ФормаДокумента", help="имя формы")
        target.add_argument("--root", default=os.path.join(REPO, "out", "preview"),
                            help="каталог с model.html (сюда пишется model.json)")
        target.add_argument("--url", default="http://127.0.0.1:8765",
                            help="адрес локального сервера")
        target.add_argument("--width", type=int, default=988)
        target.add_argument("--height", type=int, default=438)

    render = sub.add_parser("render", help="снять форму в рендерере")
    add_render_options(render)
    render.add_argument("--out", default=os.path.join(REPO, "out", "ours.png"))
    render.set_defaults(handler=command_render)

    marks = sub.add_parser("marks", help="напечатать вертикальные границы на строках")
    marks.add_argument("--image", required=True)
    marks.add_argument("--ref", default=DEFAULT_REF, help="эталон ('' чтобы не печатать)")
    marks.add_argument("--rows", required=True, help="строки через запятую")
    marks.set_defaults(handler=command_marks)

    pair = sub.add_parser("pair", help="склеить эталон и снимок")
    pair.add_argument("--ours", required=True)
    pair.add_argument("--ref", default=DEFAULT_REF)
    pair.add_argument("--out", default=os.path.join(REPO, "out", "pair.png"))
    pair.add_argument("--width", type=int, default=988)
    pair.add_argument("--top", type=int, default=0)
    pair.add_argument("--bottom", type=int, default=438)
    pair.set_defaults(handler=command_pair)

    serve = sub.add_parser("serve", help="локальный сервер без кеша")
    serve.add_argument("--root", default=os.path.join(REPO, "out", "preview"))
    serve.add_argument("--port", type=int, default=8765)
    serve.set_defaults(handler=command_serve)

    sync = sub.add_parser("sync", help="разложить ресурсы рендерера в каталог раздачи")
    sync.add_argument("--root", default=os.path.join(REPO, "out", "preview"))
    sync.set_defaults(handler=command_sync)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    args.handler(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
