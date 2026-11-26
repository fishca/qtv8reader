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
import tempfile
import time
import urllib.parse
import urllib.request
import uuid

from console_utf8 import use_utf8_stdio

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
    """Команда сборки модели. Имя файла — ``args.model``, если оно задано.

    Имя по умолчанию (``model.json``) годится для одиночного снятия. Замер по
    всем формам и любая параллельная съёмка пишут модель под своим именем:
    иначе два снимка делят один файл, и страница успевает прочитать его
    недописанным.
    """
    name = getattr(args, "model", "") or "model.json"
    return [CLI, "form-model", args.cf, args.object, "--form", args.form,
            "-o", os.path.join(args.root, name)]


def _copy_if_changed(source: str, target: str) -> bool:
    """Копирует файл, только если содержимое отличается. True — копировали.

    Перезапись поверх уже лежащего файла — не бесплатная операция: Chrome
    читает ``std-pictures/`` и ``model.json`` параллельно с копированием, а
    ``shutil.copy2`` сначала обрезает файл и лишь потом пишет в него. Читатель,
    попавший в этот промежуток, получает недописанный файл, и снимок показывает
    подставной значок вместо платформенной картинки (кнопка «Переместить вниз»
    в «АвансовомОтчёте» уходила в серый контур). Поэтому копия делается только
    при реальном расхождении, а не «на всякий случай».
    """
    try:
        if os.path.getsize(source) == os.path.getsize(target):
            with open(source, "rb") as left, open(target, "rb") as right:
                if left.read() == right.read():
                    return False
    except OSError:
        pass
    shutil.copy2(source, target)
    return True


def sync_resources(root: str) -> tuple:
    """Копирует ресурсы рендерера в каталог раздачи.

    Снимок идёт по файлам из ``--root``, а правки делаются в
    ``resources/formpreview``. Без этой копии можно смотреть на старый CSS или
    старый скрипт и «не видеть» только что сделанное исправление.

    Возвращает пару ``(скопировано, уже совпадало)``: второе число —
    насколько каталог раздачи уже соответствовал исходникам.
    """
    if not os.path.isdir(FORM_RESOURCES):
        return (0, 0)
    copied = 0
    unchanged = 0

    def place(source: str, target: str) -> None:
        nonlocal copied, unchanged
        if _copy_if_changed(source, target):
            copied += 1
        else:
            unchanged += 1

    for name in ("model.html", "icons.svg", "LICENSE-BslEdit-MIT.txt"):
        source = os.path.join(FORM_RESOURCES, name)
        if os.path.isfile(source):
            place(source, os.path.join(root, name))
    for folder, suffixes in (("css", (".css",)), ("js", (".js",)), ("std-pictures", (".png", ".svg"))):
        source_dir = os.path.join(FORM_RESOURCES, folder)
        if not os.path.isdir(source_dir):
            continue
        target_dir = os.path.join(root, folder)
        os.makedirs(target_dir, exist_ok=True)
        for name in sorted(os.listdir(source_dir)):
            source = os.path.join(source_dir, name)
            if os.path.isfile(source) and name.lower().endswith(suffixes):
                place(source, os.path.join(target_dir, name))
    for name in sorted(os.listdir(FORM_RESOURCES)):
        if name.startswith("platform-") and name.lower().endswith(".png"):
            place(os.path.join(FORM_RESOURCES, name), os.path.join(root, name))
    return (copied, unchanged)


class EmptyShotError(RuntimeError):
    """Кадр снялся пустым: страница не нарисовала форму.

    Chrome иногда отдаёт одноцветное полотно — запрос к ``model.html`` или
    к ресурсу рендерера срывается, и страница остаётся белой. Сравнивать
    такой кадр с эталоном нельзя: расхождение получается по всему полотну
    (в прогоне 0.13.1 форма «ПоляВвода» дала 8710 px структуры вместо 15,
    и замер объявлял регрессию на ровном месте). Поэтому пустота — это
    ошибка замера, а не «форма стала хуже».
    """


BLANK_SHARE = 0.995
"""Доля пикселей одного цвета, при которой кадр считается пустым.

0.995 полотна 342x189 (64 638 px) — это 323 px «чужого» цвета. Пустое
полотно даёт ноль, нарисованная форма — тысячи (рамки, подписи, полосы
прокрутки), так что порог стоит с большим запасом посередине.
"""


def frame_is_blank(path: str, share: float = BLANK_SHARE) -> bool:
    """Пустой кадр — почти всё полотно одного цвета (обычно белого).

    Считать по числу цветов нельзя: на реальной форме их много от
    антиалиасинга и ClearType, а на пустом полотне ровно один. Поэтому
    смотрим на долю самого частого цвета.

    Ловится полный провал отрисовки; частичный (нарисовалось всё, кроме
    одной картинки) этим признаком не виден — его показывает только
    расхождение с эталоном.
    """
    import numpy as np
    from PIL import Image

    image = np.asarray(Image.open(path).convert("RGB"), dtype=np.uint8)
    flat = image.reshape(-1, 3)
    if flat.size == 0:
        return True
    _, counts = np.unique(flat, axis=0, return_counts=True)
    return int(counts.max()) >= share * flat.shape[0]


def shoot(args, out_path: str, attempts: int = 2) -> None:
    """Снимает форму в окне ровно ``args.width``x``args.height``.

    Пустой кадр переснимается. Причина — одиночный сбойный запрос к раздаче
    (он же давал подставной значок вместо платформенной картинки): страница
    остаётся белой, и без повтора замер выдаёт пустое полотно за результат.
    ``attempts`` — сколько всего попыток; если пустота повторилась,
    поднимается ``EmptyShotError``, то есть замер не состоялся.
    """
    attempts = max(1, int(attempts))
    for attempt in range(1, attempts + 1):
        _capture_once(args, out_path)
        if not frame_is_blank(out_path):
            return
        print("снимок пуст (попытка %d из %d): %s"
              % (attempt, attempts, os.path.basename(out_path)), file=sys.stderr)
    raise EmptyShotError("форма не отрисовалась за %d попытки: %s"
                         % (attempts, out_path))


def _capture_once(args, out_path: str) -> None:
    """Один запуск Chrome: снимает страницу рамки и обрезает до полотна.

    Страница рендерера снимается внутри рамки фиксированного размера, а не
    напрямую: флаг ``--window-size`` задаёт размер *окна*, и Chrome на Windows
    не даёт окну стать уже ~500 px. При эталоне 342x189 область просмотра
    выходила 500x189, снимок обрезался слева до 342, и всё правое (кнопка
    автопанели «Ещё») уезжало за кадр: на каждой форме это давало ~360 px
    «расхождения» из ничего. Замер _ruler.html подтверждает: без рамки
    маркер ``right:0`` не попадает в снимок, в рамке 342x189 — попадает.
    """
    from PIL import Image

    out_path = os.path.abspath(out_path)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    if os.path.exists(out_path):
        os.remove(out_path)
    stamp = str(int(time.time()))
    root = os.path.abspath(getattr(args, "root", None)
                           or os.path.join(REPO, "out", "preview"))
    os.makedirs(root, exist_ok=True)
    frame = os.path.join(root, "_shot-%s.html" % uuid.uuid4().hex[:12])
    width, height = int(args.width), int(args.height)
    query = "model.html?t=%s" % stamp
    model_name = getattr(args, "model", "") or ""
    if model_name:
        query += "&model=" + urllib.parse.quote(model_name)
    with open(frame, "w", encoding="utf-8") as handle:
        handle.write(
            "<!doctype html>\n<html lang=\"ru\"><head><meta charset=\"utf-8\">\n"
            "<style>html,body{margin:0;padding:0;overflow:hidden;background:#fff}\n"
            "iframe{border:0;display:block;width:%dpx;height:%dpx}</style></head>\n"
            "<body><iframe src=\"%s\"></iframe></body></html>\n"
            % (width, height, query))
    raw = out_path + ".raw.png"
    if os.path.exists(raw):
        os.remove(raw)
    profile = tempfile.mkdtemp(prefix="qv8-shot-")
    try:
        subprocess.run(
            # Без --hide-scrollbars: приложение рисует собственную полосу
            # прокрутки формы (18 px, см. qv8-host.css), и место под неё
            # входит в ширину полотна. С флагом Chromium дорожку не занимает,
            # полотно выходит шире эталона, и снимок перестаёт мерить то, что
            # видно в программе.
            #
            # Окно берётся заметно больше полотна: оно нужно только как
            # поверхность для рамки, а размер страницы задаёт сама рамка.
            #
            # --virtual-time-budget: кадр снимается после загрузки страницы, а
            # model-app.js ставит __QV8_READY позже — когда раскодируются все
            # <img>. Без бюджета снимок иногда ловил пустые кнопки-картинки
            # (Дт/Кт, ↑↓, календарь): расхождение с эталоном гуляло на ±430 px.
            [find_chrome(), "--headless=new", "--disable-gpu",
             "--force-device-scale-factor=1",
             "--virtual-time-budget=8000",
             "--user-data-dir=" + profile,
             "--window-size=%d,%d" % (max(width, 900), max(height, 700)),
             "--screenshot=" + raw,
             "%s/%s?t=%s" % (args.url, os.path.basename(frame), stamp)],
            capture_output=True, env=qt_env(), timeout=180, check=False,
        )
        if not os.path.exists(raw):
            raise SystemExit("снимок не создан: " + out_path)
        image = Image.open(raw).convert("RGB")
        if image.size != (width, height):
            image = image.crop((0, 0, min(width, image.size[0]),
                                min(height, image.size[1])))
        image.save(out_path)
    finally:
        if os.path.exists(raw):
            os.remove(raw)
        if os.path.exists(frame):
            os.remove(frame)
        shutil.rmtree(profile, ignore_errors=True)
    if not os.path.exists(out_path):
        raise SystemExit("снимок не создан: " + out_path)


def command_render(args) -> None:
    if not os.path.exists(CLI):
        raise SystemExit("сначала соберите проект: " + CLI)
    out_path = os.path.abspath(args.out)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    os.makedirs(args.root, exist_ok=True)
    sync_resources(args.root)
    # Своя модель на каждый снимок: параллельные снятия не должны делить файл.
    args.model = "model-%s.json" % uuid.uuid4().hex[:12]
    model_path = os.path.join(args.root, args.model)
    try:
        result = subprocess.run(model_command(args), capture_output=True, text=True,
                                encoding="utf-8", errors="replace", env=qt_env())
        if result.returncode != 0:
            raise SystemExit("form-model: " + (result.stdout + result.stderr).strip())
        try:
            urllib.request.urlopen(args.url + "/" + args.model, timeout=5).read()
        except Exception as error:  # noqa: BLE001
            raise SystemExit("сервер не отвечает (%s): %s" % (args.url, error))
        shoot(args, out_path)
    finally:
        if os.path.exists(model_path):
            os.remove(model_path)
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
    copied, unchanged = sync_resources(os.path.abspath(args.root))
    print("разложено файлов: %d, уже совпадало: %d" % (copied, unchanged))


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
    # Раздача многопоточная: каждый снимок — отдельный Chrome с шестью
    # соединениями на узел, а замер по всем формам может идти несколькими
    # процессами сразу. На однопоточном TCPServer запросы встают в очередь, и
    # отдельная картинка успевает не дождаться ответа: страница показывает
    # подставной значок вместо платформенной картинки, и расхождение с
    # эталоном гуляет на несколько десятков пикселей.
    class Server(http.server.ThreadingHTTPServer):
        daemon_threads = True

    with Server(("127.0.0.1", args.port), Handler) as server:
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
    use_utf8_stdio()
    args = build_parser().parse_args()
    try:
        args.handler(args)
    except EmptyShotError as error:
        print("ошибка замера: %s" % error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
