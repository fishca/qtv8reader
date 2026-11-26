#!/usr/bin/env python3
"""UTF-8 для вывода инструментов.

На англоязычном Windows (cp1252) ``stdout``, отданный в трубу или файл,
получает кодировку платформы, и печать кириллицы падает с
``UnicodeEncodeError``: проверка успевает пройти, а команда (или ``ctest``)
кончается ненулевым кодом. Это уже случилось с гейтом ``control_fixtures``
в CI — с тех пор все инструменты переводят вывод в UTF-8 первой строкой
``main()``::

    from console_utf8 import use_utf8_stdio

    def main() -> int:
        use_utf8_stdio()
        ...

Признак ``isatty()`` здесь не годится: на Windows поток, отданный в
``DEVNULL``, отвечает «да» (устройство NUL считается терминалом), и проверка
оставляла бы кодировку cp1252 — именно на этом падал сервер раздачи из
``control_compare.py``. Поэтому поток переводится в UTF-8 всегда; консоль
от этого не страдает — на Windows Python и так пишет в неё через консольный
API в UTF-8.
"""

from __future__ import annotations

import io
import sys


def use_utf8_stdio() -> None:
    """Перевести ``stdout``/``stderr`` в UTF-8 (best effort)."""
    for name in ("stdout", "stderr"):
        stream = getattr(sys, name, None)
        if stream is None:
            continue
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
            continue
        except AttributeError:
            pass
        except (OSError, ValueError):
            # Закрытый или особый поток: менять нечего, печать не наша забота.
            continue
        # python < 3.7: reconfigure нет, пересобираем обёртку над буфером.
        buffer = getattr(stream, "buffer", None)
        if buffer is not None:
            setattr(sys, name, io.TextIOWrapper(
                buffer, encoding="utf-8", errors="replace",
                line_buffering=True))
