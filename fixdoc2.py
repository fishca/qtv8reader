import io
path = r"docs\form-exact-view.md"
text = io.open(path, encoding="utf-8").read()
old = "  26 форм даёт 353 расхождения по 81 свойству (в начале разбора свойств было"
new = "  26 форм даёт 346 расхождений по 81 свойству (в начале разбора свойств было"
assert old in text
text = text.replace(old, new)
anchor = """Пределы авторазмера декорации лежат не в записи макета, а в самой записи
элемента:"""
add = """Размер картинки декорации (`PictureSize`) — третье поле записи оформления
`{4,…}`: 0 — не задан (платформа свойства не пишет), 1 — `Stretch`,
2 — `Proportionally`. Сверено с `Form.xml` по 39 декорациям-картинкам корпуса.
У поля-картинки размер в этой записи не лежит: единственный случай (`AutoSize`
на форме «ЗагрузкаДокументовИзФайлов») остаётся неразобранным.

"""
assert anchor in text
text = text.replace(anchor, add + anchor)
io.open(path, "w", encoding="utf-8", newline="\n").write(text)
print("ok")
