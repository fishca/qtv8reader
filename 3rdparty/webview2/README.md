# Microsoft Edge WebView2 SDK

В каталоге лежат компоненты SDK, нужные для «точного вида» форм:

- `include/WebView2.h` — заголовок SDK (объявления COM-интерфейсов);
- `x64/WebView2Loader.dll` — загрузчик, экспортирует
  `CreateCoreWebView2EnvironmentWithOptions`. Кладётся рядом с `qtv8reader.exe`
  на этапе сборки (`CMakeLists.txt`, POST_BUILD).
- `LICENSE.txt`, `NOTICE.txt` — условия поставки от Microsoft.

Версия: `Microsoft.Web.WebView2 1.0.3351.48` (NuGet-пакет Microsoft).

SDK используется только на Windows. `WebView2Loader.dll` загружается через
`LoadLibrary`, поэтому линковать `.lib` не нужно: если DLL или сам рантайм
WebView2 отсутствуют, приложение продолжает работать, а точный вид просто
недоступен.

WebView2 Runtime (сам браузерный движок) в комплект не входит: он
устанавливается отдельно (Evergreen Runtime, входит в Windows 10/11 и
Microsoft Edge). Актуальный установщик —
<https://developer.microsoft.com/microsoft-edge/webview2/>.

Обновление SDK: скачать актуальный NuGet-пакет `Microsoft.Web.WebView2`,
заменить `WebView2.h` и `WebView2Loader.dll` (x64), проверить сборку.
