#!/usr/bin/env python3
"""Захват эталонов форм из конфигуратора 1С.

Для каждой тестовой формы скрипт открывает её в конфигураторе, находит на
экране полотно предпросмотра (оно обведено синей рамкой 2 px цвета
``#1E90FF``), обрезает ровно по внутреннему краю рамки и сохраняет PNG в
``tests/fixtures/controlbase/refs``. Размер получившейся картинки — это размер
полотна формы, он же попадает в манифест и используется сверкой
(``tools/control_compare.py``).

Как открывается форма (проверено на 8.3.27):

1. в поле «Глобальный поиск» панели инструментов печатается имя формы и
   нажимается Enter — поиск выбирает объект в дереве конфигурации;
2. F6 передаёт фокус дереву, Enter открывает выбранную форму;
3. ожидается синяя рамка полотна, снимок обрезается по ней;
4. Ctrl+F4 закрывает редактор формы, и цикл повторяется для следующей формы.

Требуется развёрнутое окно конфигуратора на экране 1920×1080 (иначе
поправьте координаты в ``refs/ui.json`` или ключом ``--calibration``).
Запуск::

    python tools/control_refs.py --launch --all
    python tools/control_refs.py --form ПоляВвода
    python tools/control_refs.py --list
"""

from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import subprocess
import sys
import time
from ctypes import wintypes

from PIL import ImageGrab
import numpy as np

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURE = os.path.join(REPO, "tests", "fixtures", "controlbase")
CONFIG_FILES = os.path.join(FIXTURE, "ConfigFiles")
REFS = os.path.join(FIXTURE, "refs")
MANIFEST = os.path.join(REFS, "manifest.json")
CALIBRATION = os.path.join(REFS, "ui.json")
DEFAULT_BASE = r"E:\work\1cv83.db\qtv8db"
DEFAULT_PLATFORM = r"C:\Program Files\1cv8\8.3.27.2342\bin\1cv8.exe"
FRAME_COLOR = (30, 144, 255)
FRAME_WIDTH = 2

DEFAULT_UI = {
    "search_field": [337, 64],
    "tree_root": [124, 166],
    "editor_title": [900, 115],
    "config_menu": [120, 29],
    "config_open_item": [200, 57],
    "result_row": [240, 728],
}

user32 = ctypes.WinDLL("user32", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

VK = {"enter": 0x0D, "escape": 0x1B, "ctrl": 0x11, "f6": 0x75, "c": 0x43,
      "f4": 0x73, "a": 0x41, "v": 0x56, "home": 0x24, "down": 0x28,
      "right": 0x27, "left": 0x25, "end": 0x23, "alt": 0x12, "m": 0x4D}

user32.SetCursorPos.argtypes = (ctypes.c_int, ctypes.c_int)
user32.GetWindowTextLengthW.argtypes = (wintypes.HWND,)
user32.GetWindowTextW.argtypes = (wintypes.HWND, wintypes.LPWSTR, ctypes.c_int)
user32.EnumChildWindows.argtypes = (wintypes.HWND, ctypes.c_void_p,
                                    wintypes.LPARAM)
user32.EnumWindows.argtypes = (ctypes.c_void_p, wintypes.LPARAM)
user32.GetWindowThreadProcessId.argtypes = (wintypes.HWND,
                                            ctypes.POINTER(wintypes.DWORD))
user32.IsWindowVisible.argtypes = (wintypes.HWND,)
user32.SetForegroundWindow.argtypes = (wintypes.HWND,)
user32.ShowWindow.argtypes = (wintypes.HWND, ctypes.c_int)
user32.GetWindowRect.argtypes = (wintypes.HWND, ctypes.POINTER(wintypes.RECT))
user32.OpenClipboard.argtypes = (wintypes.HWND,)
user32.OpenClipboard.restype = wintypes.BOOL
user32.SetClipboardData.argtypes = (wintypes.UINT, ctypes.c_void_p)
user32.SetClipboardData.restype = ctypes.c_void_p
kernel32.GlobalAlloc.argtypes = (wintypes.UINT, ctypes.c_size_t)
kernel32.GlobalAlloc.restype = ctypes.c_void_p
kernel32.GlobalLock.argtypes = (ctypes.c_void_p,)
kernel32.GlobalLock.restype = ctypes.c_void_p
kernel32.GlobalUnlock.argtypes = (ctypes.c_void_p,)

ENUMPROC = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


# --------------------------------------------------------------------------
# WinAPI: окна, мышь, клавиатура, буфер обмена


def window_title(handle) -> str:
    length = user32.GetWindowTextLengthW(handle)
    buffer = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(handle, buffer, length + 1)
    return buffer.value


def window_pid(handle) -> int:
    pid = wintypes.DWORD()
    user32.GetWindowThreadProcessId(handle, ctypes.byref(pid))
    return pid.value


def window_rect(handle):
    rect = wintypes.RECT()
    user32.GetWindowRect(handle, ctypes.byref(rect))
    return rect.left, rect.top, rect.right, rect.bottom


def top_level_windows():
    found = []

    def callback(handle, _param):
        if user32.IsWindowVisible(handle):
            found.append(handle)
        return True

    user32.EnumWindows(ENUMPROC(callback), 0)
    return found


def find_designer():
    """Окно конфигуратора: процесс 1cv8 с заголовком «…Конфигуратор…»."""
    for handle in top_level_windows():
        title = window_title(handle)
        if "Конфигуратор" in title:
            return handle, title
    return None, ""


def child_titles(handle) -> list:
    titles = []

    def callback(child, _param):
        text = window_title(child)
        if text:
            titles.append(text)
        return True

    user32.EnumChildWindows(handle, ctypes.cast(ENUMPROC(callback),
                                                ctypes.c_void_p), 0)
    return titles


def click(x: int, y: int) -> None:
    user32.SetCursorPos(int(x), int(y))
    time.sleep(0.08)
    user32.mouse_event(0x0002, 0, 0, 0, 0)
    time.sleep(0.05)
    user32.mouse_event(0x0004, 0, 0, 0, 0)
    time.sleep(0.15)


def key(vk: int, ctrl: bool = False, shift: bool = False,
        alt: bool = False) -> None:
    if ctrl:
        user32.keybd_event(VK["ctrl"], 0, 0, 0)
    if alt:
        user32.keybd_event(VK["alt"], 0, 0, 0)
    if shift:
        user32.keybd_event(0x10, 0, 0, 0)
    time.sleep(0.02)
    user32.keybd_event(vk, 0, 0, 0)
    time.sleep(0.03)
    user32.keybd_event(vk, 0, 2, 0)
    if shift:
        user32.keybd_event(0x10, 0, 2, 0)
    if alt:
        user32.keybd_event(VK["alt"], 0, 2, 0)
    if ctrl:
        user32.keybd_event(VK["ctrl"], 0, 2, 0)
    time.sleep(0.06)


def key_text(text: str) -> None:
    """Текст в фокус — через буфер обмена (русские буквы без раскладки)."""
    set_clipboard(text)
    key(VK["v"], ctrl=True)


def set_clipboard(text: str) -> None:
    CF_UNICODETEXT = 13
    GMEM_MOVEABLE = 0x0002
    data = text.encode("utf-16-le") + b"\x00\x00"
    user32.OpenClipboard(None)
    try:
        user32.EmptyClipboard()
        handle = kernel32.GlobalAlloc(GMEM_MOVEABLE, len(data))
        pointer = kernel32.GlobalLock(handle)
        ctypes.memmove(pointer, data, len(data))
        kernel32.GlobalUnlock(handle)
        user32.SetClipboardData(CF_UNICODETEXT, handle)
    finally:
        user32.CloseClipboard()


# --------------------------------------------------------------------------
# Полотно и снимок


def screenshot():
    return ImageGrab.grab()


def frame_rect(image, min_x: int = 500, min_y: int = 80):
    """Прямоугольник рамки полотна: bounding box пикселей цвета рамки."""
    array = np.asarray(image.convert("RGB"))
    mask = ((array[:, :, 0] == FRAME_COLOR[0]) & (array[:, :, 1] == FRAME_COLOR[1])
            & (array[:, :, 2] == FRAME_COLOR[2]))
    mask[:min_y, :] = False
    mask[:, :min_x] = False
    rows = np.nonzero(mask.any(axis=1))[0]
    cols = np.nonzero(mask.any(axis=0))[0]
    if not len(rows) or not len(cols):
        return None
    x0, y0, x1, y1 = int(cols.min()), int(rows.min()), int(cols.max()), int(rows.max())
    # рамка должна быть замкнутым прямоугольником толщиной FRAME_WIDTH
    top = int(mask[y0:y0 + FRAME_WIDTH, x0:x1 + 1].sum())
    bottom = int(mask[y1 - FRAME_WIDTH + 1:y1 + 1, x0:x1 + 1].sum())
    left = int(mask[y0:y1 + 1, x0:x0 + FRAME_WIDTH].sum())
    right = int(mask[y0:y1 + 1, x1 - FRAME_WIDTH + 1:x1 + 1].sum())
    expected = (x1 - x0 + 1) * FRAME_WIDTH
    if top < expected * 0.95 or bottom < expected * 0.95:
        return None
    if left < (y1 - y0 + 1) * FRAME_WIDTH * 0.95:
        return None
    return x0, y0, x1, y1


def wait_for_frame(timeout: float = 12.0, min_x: int = 500, min_y: int = 80):
    started = time.time()
    while time.time() - started < timeout:
        image = screenshot()
        rect = frame_rect(image, min_x, min_y)
        if rect:
            return image, rect
        time.sleep(0.35)
    return None, None


def crop_canvas(image, rect):
    x0, y0, x1, y1 = rect
    return image.crop((x0 + FRAME_WIDTH, y0 + FRAME_WIDTH,
                       x1 - FRAME_WIDTH + 1, y1 - FRAME_WIDTH + 1))


# --------------------------------------------------------------------------
# Формы


def fixture_forms() -> list:
    """Имена форм из выгрузки фикстуры (платформа — источник истины)."""
    names = []
    root = os.path.join(CONFIG_FILES, "DataProcessors")
    if not os.path.isdir(root):
        return names
    for processor in sorted(os.listdir(root)):
        forms_dir = os.path.join(root, processor, "Forms")
        if not os.path.isdir(forms_dir):
            continue
        for name in sorted(os.listdir(forms_dir)):
            if name.endswith(".xml"):
                names.append(name[:-4])
    return names


def load_calibration(path: str) -> dict:
    data = dict(DEFAULT_UI)
    if os.path.isfile(path):
        with open(path, encoding="utf-8") as handle:
            data.update(json.load(handle))
    return data


def ensure_designer(platform: str, base: str, ui: dict):
    handle, title = find_designer()
    if handle:
        user32.ShowWindow(handle, 3)
        user32.SetForegroundWindow(handle)
        return handle
    executable = platform
    if not os.path.isfile(executable):
        raise SystemExit("нет 1cv8.exe: " + executable)
    subprocess.Popen([executable, "DESIGNER", "/F" + base], close_fds=True)
    started = time.time()
    while time.time() - started < 120:
        handle, title = find_designer()
        if handle:
            time.sleep(3)
            user32.ShowWindow(handle, 3)
            user32.SetForegroundWindow(handle)
            time.sleep(0.5)
            # открыть окно конфигурации, если его нет
            if "Конфигурация" not in " ".join(child_titles(handle) or []):
                click(*ui["config_menu"])
                time.sleep(0.6)
                click(*ui["config_open_item"])
                time.sleep(2.5)
            return handle
        time.sleep(1)
    raise SystemExit("конфигуратор не запустился")


def close_editors(handle, limit: int = 4) -> None:
    for _ in range(limit):
        if not editor_windows(handle):
            return
        key(VK["f4"], ctrl=True)
        time.sleep(0.6)


def open_form_by_name(handle, form: str, ui: dict) -> None:
    """Открыть форму через фильтр дерева конфигурации.

    Последовательность (проверена на 8.3.27): Ctrl+Shift+C — окно
    конфигурации; Ctrl+Alt+M — фильтр дерева; имя формы; клик по корневой
    строке даёт фокус дереву; End выбирает последний видимый элемент (в
    отфильтрованном дереве это ровно найденная форма); Enter открывает её.
    """
    user32.SetForegroundWindow(handle)
    time.sleep(0.2)
    key(VK["escape"])
    time.sleep(0.15)
    close_editors(handle)
    key(VK["c"], ctrl=True, shift=True)   # окно конфигурации
    time.sleep(0.6)
    key(VK["m"], ctrl=True, alt=True)     # фильтр дерева
    time.sleep(0.4)
    key(VK["a"], ctrl=True)
    time.sleep(0.1)
    key_text(form)
    time.sleep(1.0)
    click(*ui["tree_root"])
    time.sleep(0.25)
    key(VK["end"])
    time.sleep(0.25)
    key(VK["enter"])


def editor_windows(handle) -> list:
    """Дочерние окна редактора формы: (hwnd, заголовок)."""
    found = []

    def callback(child, _param):
        text = window_title(child)
        if "Контролы:" in text:
            found.append((child, text))
        return True

    user32.EnumChildWindows(handle, ctypes.cast(ENUMPROC(callback),
                                                ctypes.c_void_p), 0)
    return found


def opened_form_title(handle) -> str:
    for _hwnd, title in editor_windows(handle):
        return title
    return ""


def maximize_editor(handle, ui: dict) -> None:
    """Развернуть редактор формы двойным щелчком по его заголовку.

    Окна 1С не отдают дочерние окна через EnumChildWindows (своя отрисовка),
    поэтому заголовок редактора находится по калиброванной точке. После
    разворачивания полотно формы помещается в панели предпросмотра целиком.
    """
    point = ui.get("editor_title", [900, 115])
    user32.SetForegroundWindow(handle)
    time.sleep(0.2)
    click(*point)
    time.sleep(0.12)
    click(*point)
    time.sleep(1.4)


def capture_form(handle, form: str, ui: dict, timeout: float):
    open_form_by_name(handle, form, ui)
    image, rect = wait_for_frame(min(timeout, 5.0))
    if not rect:
        maximize_editor(handle, ui)
        image, rect = wait_for_frame(timeout)
    if not rect:
        debug_dir = os.path.join(REPO, "out", "controlbase")
        os.makedirs(debug_dir, exist_ok=True)
        screenshot().save(os.path.join(debug_dir, "fail-" + form + ".png"))
        return None, "рамка полотна не найдена"
    canvas = crop_canvas(image, rect)
    return canvas, ""


def close_editor(handle) -> None:
    user32.SetForegroundWindow(handle)
    time.sleep(0.15)
    key(VK["f4"], ctrl=True)
    time.sleep(0.8)


def load_manifest() -> dict:
    if os.path.isfile(MANIFEST):
        with open(MANIFEST, encoding="utf-8") as handle:
            return json.load(handle)
    return {"platform": "", "base": "", "forms": {}}


def save_manifest(manifest: dict) -> None:
    with open(MANIFEST, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, ensure_ascii=False, indent=1, sort_keys=True)
        handle.write("\n")


def save_canvas(canvas, form: str) -> dict:
    os.makedirs(REFS, exist_ok=True)
    path = os.path.join(REFS, form + ".png")
    canvas.save(path)
    data = open(path, "rb").read()
    return {"file": form + ".png", "width": canvas.size[0],
            "height": canvas.size[1],
            "sha256": hashlib.sha256(data).hexdigest(),
            "captured": time.strftime("%Y-%m-%d %H:%M:%S")}


def command_list(args) -> None:
    forms = fixture_forms()
    manifest = load_manifest()
    captured = manifest.get("forms", {})
    print("форм в фикстуре: %d, эталонов: %d" % (len(forms), len(captured)))
    for form in forms:
        entry = captured.get(form)
        if entry:
            print("  %-24s %3dx%-3d %s" % (form, entry["width"], entry["height"],
                                           entry["captured"]))
        else:
            print("  %-24s  — нет эталона" % form)


def command_capture(args) -> None:
    ui = load_calibration(args.calibration)
    forms = fixture_forms() if args.all else [args.form]
    if not forms or forms == [None]:
        raise SystemExit("укажите --form или --all")
    handle = ensure_designer(args.platform, args.base, ui)
    manifest = load_manifest()
    manifest["platform"] = args.platform_version
    manifest["base"] = args.base
    os.makedirs(REFS, exist_ok=True)
    with open(CALIBRATION, "w", encoding="utf-8") as handle_file:
        json.dump(ui, handle_file, ensure_ascii=False, indent=1)
        handle_file.write("\n")
    failures = 0
    for form in forms:
        canvas, error = capture_form(handle, form, ui, args.timeout)
        close_editor(handle)
        if error:
            print("  %-24s ошибка: %s" % (form, error))
            failures += 1
            continue
        entry = save_canvas(canvas, form)
        manifest["forms"][form] = entry
        save_manifest(manifest)
        print("  %-24s %3dx%-3d сохранён" % (form, entry["width"], entry["height"]))
    print("эталоны: %d, ошибок: %d → %s" % (len(manifest["forms"]), failures, REFS))
    if failures:
        raise SystemExit(1)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--base", default=DEFAULT_BASE)
    parser.add_argument("--platform", default=DEFAULT_PLATFORM)
    parser.add_argument("--platform-version", default="8.3.27.2342")
    parser.add_argument("--calibration", default=CALIBRATION)
    parser.add_argument("--timeout", type=float, default=12.0)
    sub = parser.add_subparsers(dest="command", required=True)
    listing = sub.add_parser("list", help="какие эталоны уже есть")
    listing.set_defaults(handler=command_list)
    capture = sub.add_parser("capture", help="снять эталоны")
    group = capture.add_mutually_exclusive_group(required=True)
    group.add_argument("--form", default=None, help="имя формы")
    group.add_argument("--all", action="store_true", help="все формы фикстуры")
    capture.set_defaults(handler=command_capture)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    args.handler(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
