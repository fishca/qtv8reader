#!/usr/bin/env python3
"""Сторож распознавания пустого снимка формы (``frame_is_blank``).

Chrome здесь не нужен: кадры рисуются самим PIL. Сторож нужен потому, что
цена ошибки высока с обеих сторон: пропущенная пустота превращается в ложную
регрессию (в прогоне 0.13.1 форма «ПоляВвода» дала 8710 px структуры вместо
15), а ложное срабатывание гоняет пересъёмку на каждой форме и в конце концов
валит замер.

    python tools/blank_shot_check.py

Без Pillow проверка пропускается с кодом 0: инструменты попиксельного замера
не входят в минимальную сборку, и ctest не должен краснеть там, где Pillow
не ставили.
"""

from __future__ import annotations

import os
import shutil
import sys
import tempfile

from console_utf8 import use_utf8_stdio

WIDTH, HEIGHT = 342, 189


def blank_frames(Image) -> list:
    """Кадры, которые обязаны считаться пустыми: полотно без содержимого."""
    grey = Image.new("RGB", (WIDTH, HEIGHT), (200, 200, 200))
    dot = Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))
    dot.putpixel((10, 10), (0, 0, 0))
    return [
        ("белое полотно", Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))),
        ("серое полотно", grey),
        # Единичный пиксель формы не несёт: такой кадр — тоже провал отрисовки.
        ("полотно с одной точкой", dot),
    ]


def drawn_frames(Image, ImageDraw) -> list:
    """Кадры, которые пустыми считать нельзя: в них есть содержимое формы."""
    border = Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))
    ImageDraw.Draw(border).rectangle(
        [0, 0, WIDTH - 1, HEIGHT - 1], outline=(140, 140, 140), width=15)

    thin = Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))
    ImageDraw.Draw(thin).rectangle(
        [0, 0, WIDTH - 1, HEIGHT - 1], outline=(140, 140, 140), width=1)

    form = Image.new("RGB", (WIDTH, HEIGHT), (255, 255, 255))
    draw = ImageDraw.Draw(form)
    draw.rectangle([0, 0, WIDTH - 1, HEIGHT - 1], outline=(140, 140, 140), width=15)
    for row in range(4):
        top = 30 + row * 30
        for x in range(20, 200, 7):  # подписи полей
            draw.rectangle([x, top, x + 4, top + 8], fill=(40, 40, 40))
        draw.rectangle([210, top - 4, 320, top + 16], outline=(120, 120, 120), width=1)
    return [
        ("рамка 15 px", border),
        # Даже контур в один пиксель — это уже форма, а не пустое полотно.
        ("тонкая рамка 1 px", thin),
        ("форма с подписями", form),
    ]


def main() -> int:
    use_utf8_stdio()
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow не установлен — проверка пустого снимка пропущена")
        return 0

    import form_view_measure as measure

    blanks = blank_frames(Image)
    drawn = drawn_frames(Image, ImageDraw)
    folder = tempfile.mkdtemp(prefix="qv8-blank-")
    problems = []
    try:
        for index, (name, image) in enumerate(blanks):
            path = os.path.join(folder, "blank-%d.png" % index)
            image.save(path)
            if not measure.frame_is_blank(path):
                problems.append("пустой кадр не распознан: " + name)
        for index, (name, image) in enumerate(drawn):
            path = os.path.join(folder, "drawn-%d.png" % index)
            image.save(path)
            if measure.frame_is_blank(path):
                problems.append("нарисованный кадр принят за пустой: " + name)
    finally:
        shutil.rmtree(folder, ignore_errors=True)

    if problems:
        print("пустой снимок: расхождений %d" % len(problems))
        for problem in problems:
            print("  " + problem)
        return 1
    print("пустой снимок распознаётся верно: кадров проверено %d"
          % (len(blanks) + len(drawn)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
