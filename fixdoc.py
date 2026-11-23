import io
path = r"docs\form-exact-view.md"
text = io.open(path, encoding="utf-8").read()
old1 = "  `UserVisible` (16), `PictureSize` (8) и пути к данным полей панели"
new1 = "  `UserVisible` (16), `PictureSize` (1 — у поля-картинки `AutoSize`;\n  у декораций-картинок размер разбирается по третьему полю записи оформления) и пути к данным полей панели"
assert old1 in text
text = text.replace(old1, new1)
old2 = """  (3 таблицы корпуса). У декорации не разобраны `PictureSize` (Stretch /
  Proportionally) и `NonselectedPictureText` — 10 свойств."""
new2 = """  (3 таблицы корпуса). У декорации не разобран `NonselectedPictureText`, а
  `PictureSize` у поля-картинки (`AutoSize`) — 2 свойства."""
assert old2 in text
text = text.replace(old2, new2)
io.open(path, "w", encoding="utf-8", newline="\n").write(text)
print("ok")
