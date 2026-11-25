#!/usr/bin/env python3
"""Сверка отрисовки форм: наш рендерер против эталона конфигуратора.

Для каждой формы, у которой есть эталон (``tests/fixtures/controlbase/refs``):

1. модель собирается командой ``v8reader_cli form-model`` из фикстурного .cf;
2. страница рендерера снимается headless-Chrome ровно в размере эталона;
3. считается расхождение попиксельно (порог на канал, как в замерах формы
   «Авансового отчёта»), пиксели делятся на **текстовые** (край глифа) и
   **структурные** (рамка, линия, заливка, значок) по мелким связным
   компонентам «чернил» эталона;
4. складывается пара «эталон / наш снимок» и тепловая карта, пишется
   ``result.json``, общий отчёт — ``out/control-report.md``.

Запуск::

    python tools/form_view_measure.py serve --root out/preview --port 8765  # один раз
    python tools/control_compare.py --all
    python tools/control_compare.py --form ПоляВвода --update-baseline
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time
import urllib.request

import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))
import form_view_measure as measure  # noqa: E402

FIXTURE = os.path.join(REPO, "tests", "fixtures", "controlbase")
REFS = os.path.join(FIXTURE, "refs")
PAGE_ROOT = os.path.join(REPO, "out", "preview")
OUT = os.path.join(REPO, "out", "control")
BASELINE = os.path.join(FIXTURE, "baseline.json")
CF = os.path.join(FIXTURE, "1Cv8_controls.cf")
OBJECT = "Обработка.Контролы"
THRESHOLD = 24


def load_manifest() -> dict:
    path = os.path.join(REFS, "manifest.json")
    if not os.path.isfile(path):
        return {"forms": {}}
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


def ensure_server(url: str, port: int) -> subprocess.Popen | None:
    try:
        urllib.request.urlopen(url + "/model.html", timeout=2).read(64)
        return None
    except Exception:  # noqa: BLE001
        pass
    process = subprocess.Popen(
        [sys.executable, os.path.join(REPO, "tools", "form_view_measure.py"),
         "serve", "--root", PAGE_ROOT, "--port", str(port)],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    for _ in range(40):
        time.sleep(0.25)
        try:
            urllib.request.urlopen(url + "/model.html", timeout=2).read(64)
            return process
        except Exception:  # noqa: BLE001
            continue
    process.kill()
    raise SystemExit("не удалось поднять локальный сервер " + url)


def render_form(form: str, width: int, height: int, url: str) -> str:
    out_dir = os.path.join(OUT, form)
    os.makedirs(out_dir, exist_ok=True)
    model_path = os.path.join(PAGE_ROOT, "model.json")
    os.makedirs(PAGE_ROOT, exist_ok=True)
    command = [measure.CLI, "form-model", CF, OBJECT, "--form", form,
               "-o", model_path]
    result = subprocess.run(command, capture_output=True, text=True,
                            encoding="utf-8", errors="replace",
                            env=measure.qt_env())
    if result.returncode != 0:
        raise SystemExit("form-model: " + (result.stdout + result.stderr).strip())
    class Args:
        pass
    args = Args()
    args.url = url
    args.width = width
    args.height = height
    ours = os.path.join(out_dir, "ours.png")
    measure.shoot(args, ours)
    return ours


def ink_components(image) -> np.ndarray:
    """Маска мелких связных компонент «чернил» — кандидатов в глифы."""
    array = np.asarray(image.convert("RGB"), dtype=np.int16)
    spread = array.max(axis=2) - array.min(axis=2)
    dark = array.max(axis=2) < 235
    ink = dark | (spread > 24)
    height, width = ink.shape
    seen = np.zeros_like(ink, dtype=bool)
    text = np.zeros_like(ink, dtype=bool)
    for y in range(height):
        for x in range(width):
            if not ink[y, x] or seen[y, x]:
                continue
            stack = [(y, x)]
            seen[y, x] = True
            pixels = []
            while stack:
                cy, cx = stack.pop()
                pixels.append((cy, cx))
                for dy in (-1, 0, 1):
                    for dx in (-1, 0, 1):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < height and 0 <= nx < width and ink[ny, nx] \
                                and not seen[ny, nx]:
                            seen[ny, nx] = True
                            stack.append((ny, nx))
            ys = [p[0] for p in pixels]
            xs = [p[1] for p in pixels]
            box_h = max(ys) - min(ys) + 1
            box_w = max(xs) - min(xs) + 1
            if len(pixels) <= 420 and box_h <= 24 and box_w <= 60:
                for py, px in pixels:
                    text[py, px] = True
    return text


def dilate(mask: np.ndarray, radius: int = 1) -> np.ndarray:
    result = mask.copy()
    for dy in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            shifted = np.zeros_like(mask)
            height, width = mask.shape
            ys = slice(max(0, dy), height + min(0, dy))
            xs = slice(max(0, dx), width + min(0, dx))
            yd = slice(max(0, -dy), height + min(0, -dy))
            xd = slice(max(0, -dx), width + min(0, -dx))
            shifted[yd, xd] = mask[ys, xs]
            result |= shifted
    return result


def diff_images(ref_path: str, ours_path: str) -> dict:
    ref_image = Image.open(ref_path).convert("RGB")
    ours_image = Image.open(ours_path).convert("RGB")
    if ref_image.size != ours_image.size:
        ours_image = ours_image.resize(ref_image.size)
    ref = np.asarray(ref_image, dtype=np.int16)
    ours = np.asarray(ours_image, dtype=np.int16)
    delta = np.abs(ref - ours).max(axis=2)
    mask = delta > THRESHOLD
    total = int(mask.sum())
    pixels = int(mask.size)
    text_candidates = dilate(ink_components(ref_image))
    text = int((mask & text_candidates).sum())
    structure = total - text
    bands = []
    rows = np.nonzero(mask.any(axis=1))[0]
    for y in rows:
        if bands and y == bands[-1][1] + 1:
            bands[-1][1] = int(y)
        else:
            bands.append([int(y), int(y)])
    return {
        "width": ref_image.size[0], "height": ref_image.size[1],
        "pixels": pixels, "total": total, "text": text, "structure": structure,
        "percent": round(100.0 * total / pixels, 2),
        "bands": [{"from": a, "to": b, "pixels": int(mask[a:b + 1].sum())}
                  for a, b in bands],
        "mask": mask,
    }


def write_images(form: str, ref_path: str, ours_path: str, mask) -> None:
    out_dir = os.path.join(OUT, form)
    ref = Image.open(ref_path).convert("RGB")
    ours = Image.open(ours_path).convert("RGB")
    width, height = ref.size
    pair = Image.new("RGB", (width, height * 2 + 6), (255, 0, 0))
    pair.paste(ref, (0, 0))
    pair.paste(ours, (0, height + 6))
    pair.save(os.path.join(out_dir, "pair.png"))
    heat = np.zeros((height, width, 3), dtype=np.uint8)
    heat[:, :] = (255, 255, 255)
    heat[mask] = (220, 30, 30)
    Image.fromarray(heat).save(os.path.join(out_dir, "diff.png"))


def process(form: str, url: str) -> dict:
    manifest = load_manifest().get("forms", {})
    entry = manifest.get(form)
    ref_path = os.path.join(REFS, form + ".png")
    if not entry or not os.path.isfile(ref_path):
        return {"form": form, "error": "нет эталона"}
    result = render_form(form, entry["width"], entry["height"], url)
    data = diff_images(ref_path, result)
    mask = data.pop("mask")
    write_images(form, ref_path, result, mask)
    data["form"] = form
    data["ours"] = os.path.relpath(result, REPO)
    data["ref"] = os.path.relpath(ref_path, REPO)
    with open(os.path.join(OUT, form, "result.json"), "w", encoding="utf-8") as handle:
        json.dump(data, handle, ensure_ascii=False, indent=1)
        handle.write("\n")
    return data


def load_baseline() -> dict:
    if os.path.isfile(BASELINE):
        with open(BASELINE, encoding="utf-8") as handle:
            return json.load(handle)
    return {"forms": {}}


def write_report(results: list, baseline: dict, update: bool) -> None:
    lines = ["# Сверка контролов с конфигуратором", "",
             "Пиксельное расхождение эталона и нашего рендера. «Структура» — "
             "рамки, линии, заливки и значки, «текст» — края глифов "
             "(растеризаторы платформы и Chromium расходятся принципиально).",
             "",
             "| Форма | Полотно | Всего px | % | Структура | Текст | Базовый текст | Статус |",
             "|---|---|---|---|---|---|---|---|"]
    total_structure = 0
    for result in results:
        if "error" in result:
            lines.append("| %s | — | — | — | — | — | — | %s |"
                         % (result["form"], result["error"]))
            continue
        form = result["form"]
        base_text = baseline.get("forms", {}).get(form)
        if update:
            baseline.setdefault("forms", {})[form] = result["text"]
            base_text = result["text"]
        total_structure += result["structure"]
        allowed = base_text is None or result["text"] <= base_text
        status = "ок" if result["structure"] == 0 and allowed else "расхождение"
        lines.append("| %s | %dx%d | %d | %.2f | %d | %d | %s | %s |"
                     % (form, result["width"], result["height"], result["total"],
                        result["percent"], result["structure"], result["text"],
                        "—" if base_text is None else str(base_text), status))
    lines += ["", "Структурных расхождений: %d. Эталоны — "
                  "`tests/fixtures/controlbase/refs`, снимки — `out/control`."
                  % total_structure, ""]
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, "control-report.md"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines))
    if update:
        with open(BASELINE, "w", encoding="utf-8") as handle:
            json.dump(baseline, handle, ensure_ascii=False, indent=1, sort_keys=True)
            handle.write("\n")
    print("отчёт: out/control/control-report.md")


def all_forms() -> list:
    return sorted(load_manifest().get("forms", {}).keys())


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--form", default=None)
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--url", default="http://127.0.0.1:8765")
    parser.add_argument("--update-baseline", action="store_true")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    forms = all_forms() if args.all else ([args.form] if args.form else [])
    if not forms:
        raise SystemExit("укажите --form или --all")
    os.makedirs(PAGE_ROOT, exist_ok=True)
    measure.sync_resources(PAGE_ROOT)
    server = ensure_server(args.url, int(args.url.rsplit(":", 1)[-1]))
    try:
        results = [process(form, args.url) for form in forms]
    finally:
        if server:
            server.kill()
    baseline = load_baseline()
    write_report(results, baseline, args.update_baseline)
    for result in results:
        if "error" in result:
            print("  %-24s %s" % (result["form"], result["error"]))
        else:
            print("  %-24s %d px (%.2f%%): структура %d, текст %d"
                  % (result["form"], result["total"], result["percent"],
                     result["structure"], result["text"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
