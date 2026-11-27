# @fishca/dsh-v8reader

Плагин DeepSeek Harness: страница правой панели, которая читает конфигурации и
внешние обработки 1С:Предприятие 8 (`.cf`, `.cfe`, `.epf`, `.erf`) через
`v8reader_cli` из корня этого репозитория.

- Дерево метаданных как в 1С: конфигурация → категория → объект → разделы и
  листья; формы и модули открываются из дерева.
- Модули — код BSL с подсветкой, номерами строк, переносом, размером шрифта и
  копированием.
- Формы — точный вид приложения в `<iframe>` с масштабом.
- Подробности и ограничения: [docs/DETAIL.md](docs/DETAIL.md).
- Контракт CLI (`view --json`, схема 1): `docs/plans/dsh-v8kit.md`, раздел 5.

## Сборка и тесты

```
npm install        # esbuild, только для сборки клиента
npm run build      # src/client -> client.js (закоммичен)
npm run assets     # resources/formpreview -> assets/formpreview (закоммичено)
npm test           # node --test: host + client + bsl
```

Хост-половина не требует сборки. Тесты интеграции берут фикстуру
`tests/fixtures/controlbase/1Cv8_controls.cf` и `v8reader_cli` (см. порядок
поиска в DETAIL.md); без них тест пропускается.
