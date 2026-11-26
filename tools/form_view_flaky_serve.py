#!/usr/bin/env python3
"""Раздача файлов рендерера с нарочным сбоем картинки.

Нужна для проверки повтора загрузки: рендерер подменяет платформенную
картинку серым контуром, если запрос провалился, и настоящий сбой
``std-pictures/`` редок и невоспроизводим. Здесь сбой задан точно: первые
``--times`` запросов указанного файла отвечают 500, дальше файл отдаётся как
обычно. Снимок через такой сервер обязан совпасть с чистым — это и есть
проверка, что повтор срабатывает, а не маскирует настоящий 404.

Запуск::

    python tools/form_view_flaky_serve.py --port 8799
    python tools/form_view_measure.py render --url http://127.0.0.1:8799 \
        --out out/tmp/retry.png
    python tools/form_view_measure.py render --out out/tmp/clean.png

Дальше два снимка сравниваются попиксельно (порог 24 на канал — как в
``control_compare.py``): отличий быть не должно, потому что повтор загрузил
картинку со второй попытки.

Проверка контура для честного 404 — та же команда ``render`` при удалённом
файле из ``resources/formpreview/std-pictures``: второй отказ тоже уходит в
контур, и расхождение с эталоном возвращается к 8 713 px.
"""

from __future__ import annotations

import argparse
import http.server
import os
import sys

from console_utf8 import use_utf8_stdio

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--root", default=os.path.join(REPO, "out", "preview"),
                        help="каталог раздачи (как у form_view_measure serve)")
    parser.add_argument("--port", type=int, default=8799)
    parser.add_argument("--target", default="/std-pictures/PeremestitVniz.png",
                        help="путь, на котором будет сбой")
    parser.add_argument("--times", type=int, default=1,
                        help="сколько первых запросов положить (по умолчанию 1)")
    return parser


def main() -> int:
    use_utf8_stdio()
    args = build_parser().parse_args()
    root = os.path.abspath(args.root)
    state = {"failed": 0}

    class Handler(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *handler_args, **handler_kwargs):
            super().__init__(*handler_args, directory=root, **handler_kwargs)

        def do_GET(self):
            if (self.path.split("?")[0] == args.target
                    and state["failed"] < args.times):
                state["failed"] += 1
                self.send_response(500)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return
            super().do_GET()

        def log_message(self, *handler_args):
            pass

    class Server(http.server.ThreadingHTTPServer):
        daemon_threads = True

    with Server(("127.0.0.1", args.port), Handler) as server:
        print("раздаю %s на http://127.0.0.1:%d (сбой: %s, повторов: %d)"
              % (root, args.port, args.target, args.times), flush=True)
        server.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(main())
