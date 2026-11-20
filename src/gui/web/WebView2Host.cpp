#include "gui/web/WebView2Host.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStandardPaths>
#include <QTime>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

#ifdef Q_OS_WIN
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>

#  include <objbase.h>
#  include <shlwapi.h>

#  include "WebView2.h"
#endif // Q_OS_WIN

namespace v8reader {

#ifdef Q_OS_WIN

namespace {

// IID обработчиков WebView2: фиксированные значения из WebView2.h.
// __uuidof() для этих интерфейсов в MinGW недоступен (в заголовке SDK нет
// __declspec(uuid)), поэтому идентификаторы заданы явно.
const GUID kIidEnvironmentCompleted = {
    0x4e8a3389, 0xc9d8, 0x4bd2, {0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d}};
const GUID kIidControllerCompleted = {
    0x6c4819f3, 0xc9b7, 0x4260, {0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c}};
const GUID kIidResourceRequested = {
    0xab00b74c, 0x15f1, 0x4646, {0x80, 0xe8, 0xe7, 0x63, 0x41, 0xd2, 0x5d, 0x71}};
const GUID kIidNavigationCompleted = {
    0xd33a35bf, 0x1c49, 0x4f98, {0x93, 0xab, 0x00, 0x6e, 0x05, 0x33, 0xfe, 0x1c}};
const GUID kIidNewWindowRequested = {
    0xd4c185fe, 0xc81c, 0x4989, {0x97, 0xaf, 0x2d, 0x3f, 0xa7, 0xab, 0x56, 0x51}};
const GUID kIidWebMessageReceived = {
    0x57213f19, 0x00e6, 0x49fa, {0x8e, 0x07, 0x89, 0x8e, 0xa0, 0x1e, 0xcb, 0xd2}};

inline LPCWSTR wide(const QString &text)
{
    return reinterpret_cast<LPCWSTR>(text.utf16());
}

// Отладочный журнал «точного вида»: пишется только если задана переменная
// окружения QTV8_WEBVIEW2_LOG (путь к файлу).
void debugLog(const QString &message)
{
    static const QString path = qEnvironmentVariable("QTV8_WEBVIEW2_LOG");
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append))
        return;
    file.write(QStringLiteral("[%1] %2\n")
                   .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss.zzz")),
                        message)
                   .toUtf8());
}

inline HRESULT queryHandler(IUnknown *self, REFIID riid, void **ppvObject, const GUID &iid)
{
    if (!ppvObject)
        return E_POINTER;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, iid)) {
        *ppvObject = self;
        self->AddRef();
        return S_OK;
    }
    *ppvObject = nullptr;
    return E_NOINTERFACE;
}

template <class Interface>
class RefCountedHandler : public Interface
{
public:
    virtual ~RefCountedHandler() = default;

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return static_cast<ULONG>(InterlockedIncrement(&m_ref));
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG value = InterlockedDecrement(&m_ref);
        if (value == 0)
            delete this;
        return static_cast<ULONG>(value);
    }

protected:
    LONG m_ref = 1;
};

class EnvironmentCompletedHandler final
    : public RefCountedHandler<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>
{
public:
    using Callback = std::function<void(HRESULT, ICoreWebView2Environment *)>;

    explicit EnvironmentCompletedHandler(Callback callback)
        : m_callback(std::move(callback))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        return queryHandler(this, riid, ppvObject, kIidEnvironmentCompleted);
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                     ICoreWebView2Environment *environment) override
    {
        Callback callback = m_callback;
        if (callback)
            callback(result, environment);
        return S_OK;
    }

private:
    Callback m_callback;
};

class ControllerCompletedHandler final
    : public RefCountedHandler<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>
{
public:
    using Callback = std::function<void(HRESULT, ICoreWebView2Controller *)>;

    explicit ControllerCompletedHandler(Callback callback)
        : m_callback(std::move(callback))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        return queryHandler(this, riid, ppvObject, kIidControllerCompleted);
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT result,
                                     ICoreWebView2Controller *controller) override
    {
        Callback callback = m_callback;
        if (callback)
            callback(result, controller);
        return S_OK;
    }

private:
    Callback m_callback;
};

class WebResourceRequestedHandler final
    : public RefCountedHandler<ICoreWebView2WebResourceRequestedEventHandler>
{
public:
    using Callback = std::function<void(ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *)>;

    explicit WebResourceRequestedHandler(Callback callback)
        : m_callback(std::move(callback))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        return queryHandler(this, riid, ppvObject, kIidResourceRequested);
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender,
                                     ICoreWebView2WebResourceRequestedEventArgs *args) override
    {
        Callback callback = m_callback;
        if (callback)
            callback(sender, args);
        return S_OK;
    }

private:
    Callback m_callback;
};

class NavigationCompletedHandler final
    : public RefCountedHandler<ICoreWebView2NavigationCompletedEventHandler>
{
public:
    using Callback = std::function<void(ICoreWebView2 *, ICoreWebView2NavigationCompletedEventArgs *)>;

    explicit NavigationCompletedHandler(Callback callback)
        : m_callback(std::move(callback))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        return queryHandler(this, riid, ppvObject, kIidNavigationCompleted);
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender,
                                     ICoreWebView2NavigationCompletedEventArgs *args) override
    {
        Callback callback = m_callback;
        if (callback)
            callback(sender, args);
        return S_OK;
    }

private:
    Callback m_callback;
};

class NewWindowRequestedHandler final
    : public RefCountedHandler<ICoreWebView2NewWindowRequestedEventHandler>
{
public:
    using Callback = std::function<void(ICoreWebView2 *, ICoreWebView2NewWindowRequestedEventArgs *)>;

    explicit NewWindowRequestedHandler(Callback callback)
        : m_callback(std::move(callback))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        return queryHandler(this, riid, ppvObject, kIidNewWindowRequested);
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender,
                                     ICoreWebView2NewWindowRequestedEventArgs *args) override
    {
        Callback callback = m_callback;
        if (callback)
            callback(sender, args);
        return S_OK;
    }

private:
    Callback m_callback;
};

class WebMessageReceivedHandler final
    : public RefCountedHandler<ICoreWebView2WebMessageReceivedEventHandler>
{
public:
    using Callback = std::function<void(ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *)>;

    explicit WebMessageReceivedHandler(Callback callback)
        : m_callback(std::move(callback))
    {
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
    {
        return queryHandler(this, riid, ppvObject, kIidWebMessageReceived);
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *sender,
                                     ICoreWebView2WebMessageReceivedEventArgs *args) override
    {
        Callback callback = m_callback;
        if (callback)
            callback(sender, args);
        return S_OK;
    }

private:
    Callback m_callback;
};

using CreateEnvironmentWithOptions = HRESULT(STDMETHODCALLTYPE *)(
    PCWSTR browserExecutableFolder, PCWSTR userDataFolder,
    ICoreWebView2EnvironmentOptions *environmentOptions,
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *environmentCreatedHandler);

const wchar_t *const kRuntimeKeyMachine =
    L"SOFTWARE\\WOW6432Node\\Microsoft\\EdgeUpdate\\Clients\\"
    L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";
const wchar_t *const kRuntimeKeyUser =
    L"SOFTWARE\\Microsoft\\EdgeUpdate\\Clients\\"
    L"{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}";

QString readRuntimeVersion(HKEY root, const wchar_t *key)
{
    HKEY handle = nullptr;
    if (RegOpenKeyExW(root, key, 0, KEY_READ | KEY_WOW64_32KEY, &handle) != ERROR_SUCCESS)
        return QString();
    wchar_t buffer[128] = {};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    QString version;
    if (RegQueryValueExW(handle, L"pv", nullptr, &type,
                         reinterpret_cast<LPBYTE>(buffer), &size)
            == ERROR_SUCCESS
        && (type == REG_SZ || type == REG_EXPAND_SZ)) {
        version = QString::fromWCharArray(buffer).trimmed();
    }
    RegCloseKey(handle);
    return version;
}

// Загружает WebView2Loader.dll из каталога приложения (или через обычный
// поиск Windows) и возвращает указатель на функцию создания среды.
CreateEnvironmentWithOptions loadCreateEnvironment(QString *error)
{
    static HMODULE loader = nullptr;
    static CreateEnvironmentWithOptions create = nullptr;
    if (create)
        return create;

    const QString local = QDir(QCoreApplication::applicationDirPath())
                              .filePath(QStringLiteral("WebView2Loader.dll"));
    if (QFileInfo::exists(local))
        loader = LoadLibraryW(wide(QDir::toNativeSeparators(local)));
    if (!loader)
        loader = LoadLibraryW(L"WebView2Loader.dll");
    if (!loader) {
        if (error) {
            *error = QStringLiteral(
                "Не найден WebView2Loader.dll (ожидался рядом с программой: %1).")
                         .arg(QDir::toNativeSeparators(local));
        }
        return nullptr;
    }
    create = reinterpret_cast<CreateEnvironmentWithOptions>(
        reinterpret_cast<void *>(GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions")));
    if (!create && error) {
        *error = QStringLiteral(
            "WebView2Loader.dll загружен, но функция "
            "CreateCoreWebView2EnvironmentWithOptions в нём отсутствует.");
    }
    return create;
}

QString runtimeVersionCached()
{
    QString version = readRuntimeVersion(HKEY_LOCAL_MACHINE, kRuntimeKeyMachine);
    if (version.isEmpty())
        version = readRuntimeVersion(HKEY_CURRENT_USER, kRuntimeKeyUser);
    return version;
}

QString profileDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (base.isEmpty())
        base = QDir::tempPath();
    return QDir(base).filePath(QStringLiteral("webview2"));
}

} // namespace

class WebView2Host::Impl
{
public:
    explicit Impl(WebView2Host *owner)
        : q(owner)
        , alive(std::make_shared<bool>(true))
    {
    }

    ~Impl()
    {
        *alive = false;
        teardown();
    }

    void teardown()
    {
        if (webView) {
            if (resourceToken.value)
                webView->remove_WebResourceRequested(resourceToken);
            if (navigationToken.value)
                webView->remove_NavigationCompleted(navigationToken);
            if (newWindowToken.value)
                webView->remove_NewWindowRequested(newWindowToken);
            if (messageToken.value)
                webView->remove_WebMessageReceived(messageToken);
        }
        resourceToken = {};
        navigationToken = {};
        newWindowToken = {};
        messageToken = {};
        if (controller) {
            controller->Close();
            controller->Release();
            controller = nullptr;
        }
        if (webView) {
            webView->Release();
            webView = nullptr;
        }
        if (environment) {
            environment->Release();
            environment = nullptr;
        }
        initializing = false;
        ready = false;
    }

    void fail(const QString &message)
    {
        Q_EMIT q->loadFailed(message);
    }

    void initialize()
    {
        if (initializing || controller)
            return;
        initializing = true;

        QString reason;
        if (!WebView2Host::isSupported(&reason)) {
            initializing = false;
            debugLog(QStringLiteral("initialize: среда недоступна: %1").arg(reason));
            fail(reason);
            return;
        }
        debugLog(QStringLiteral("initialize: среда WebView2 %1")
                     .arg(WebView2Host::runtimeVersion()));
        CreateEnvironmentWithOptions create = loadCreateEnvironment(&reason);
        if (!create) {
            initializing = false;
            debugLog(QStringLiteral("initialize: загрузчик недоступен: %1").arg(reason));
            fail(reason);
            return;
        }

        const QString profile = profileDirectory();
        QDir().mkpath(profile);
        const QString nativeProfile = QDir::toNativeSeparators(profile);

        std::shared_ptr<bool> guard = alive;
        HRESULT result = create(
            nullptr, wide(nativeProfile), nullptr,
            new EnvironmentCompletedHandler([this, guard](HRESULT status,
                                                          ICoreWebView2Environment *env) {
                if (!*guard)
                    return;
                onEnvironmentCreated(status, env);
            }));
        if (FAILED(result)) {
            initializing = false;
            fail(QStringLiteral("Не удалось инициализировать WebView2 (HRESULT 0x%1).")
                     .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0')));
        }
    }

    void onEnvironmentCreated(HRESULT status, ICoreWebView2Environment *env)
    {
        debugLog(QStringLiteral("onEnvironmentCreated: hr=0x%1 env=%2")
                     .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'))
                     .arg(env ? QStringLiteral("да") : QStringLiteral("нет")));
        if (FAILED(status) || !env) {
            initializing = false;
            fail(QStringLiteral("WebView2 не запустился (HRESULT 0x%1). Проверьте, "
                                "что установлен Microsoft Edge WebView2 Runtime.")
                     .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0')));
            return;
        }
        environment = env;
        environment->AddRef();

        HWND hwnd = reinterpret_cast<HWND>(q->winId());
        std::shared_ptr<bool> guard = alive;
        HRESULT result = environment->CreateCoreWebView2Controller(
            hwnd, new ControllerCompletedHandler([this, guard](HRESULT controllerStatus,
                                                               ICoreWebView2Controller *ctrl) {
                if (!*guard)
                    return;
                onControllerCreated(controllerStatus, ctrl);
            }));
        if (FAILED(result)) {
            initializing = false;
            fail(QStringLiteral("Не удалось создать WebView2 (HRESULT 0x%1).")
                     .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0')));
        }
    }

    void onControllerCreated(HRESULT status, ICoreWebView2Controller *ctrl)
    {
        debugLog(QStringLiteral("onControllerCreated: hr=0x%1 ctrl=%2")
                     .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'))
                     .arg(ctrl ? QStringLiteral("да") : QStringLiteral("нет")));
        if (FAILED(status) || !ctrl) {
            initializing = false;
            fail(QStringLiteral("WebView2 не создал область просмотра (HRESULT 0x%1).")
                     .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0')));
            return;
        }
        controller = ctrl;
        controller->AddRef();
        if (FAILED(controller->get_CoreWebView2(&webView)) || !webView) {
            initializing = false;
            fail(QStringLiteral("WebView2 не предоставил ICoreWebView2."));
            return;
        }

        if (ICoreWebView2Settings *settings = nullptr;
            SUCCEEDED(webView->get_Settings(&settings)) && settings) {
            // Контекстное меню браузера в просмотрщике формы не нужно,
            // а масштаб выставляется из окна программы.
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_IsZoomControlEnabled(FALSE);
            settings->put_AreDevToolsEnabled(TRUE);
            settings->put_IsBuiltInErrorPageEnabled(FALSE);
            settings->Release();
        }

        webView->AddWebResourceRequestedFilter(L"https://qv8.local/*",
                                               COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

        std::shared_ptr<bool> guard = alive;
        webView->add_WebResourceRequested(
            new WebResourceRequestedHandler(
                [this, guard](ICoreWebView2 *, ICoreWebView2WebResourceRequestedEventArgs *args) {
                    if (!*guard)
                        return;
                    handleResourceRequest(args);
                }),
            &resourceToken);
        webView->add_NavigationCompleted(
            new NavigationCompletedHandler(
                [this, guard](ICoreWebView2 *,
                              ICoreWebView2NavigationCompletedEventArgs *args) {
                    if (!*guard)
                        return;
                    onNavigationCompleted(args);
                }),
            &navigationToken);
        webView->add_NewWindowRequested(
            new NewWindowRequestedHandler(
                [this, guard](ICoreWebView2 *,
                              ICoreWebView2NewWindowRequestedEventArgs *args) {
                    if (!*guard)
                        return;
                    if (args)
                        args->put_Handled(TRUE);
                }),
            &newWindowToken);
        webView->add_WebMessageReceived(
            new WebMessageReceivedHandler(
                [this, guard](ICoreWebView2 *,
                              ICoreWebView2WebMessageReceivedEventArgs *args) {
                    if (!*guard || !args)
                        return;
                    LPWSTR json = nullptr;
                    if (SUCCEEDED(args->get_WebMessageAsJson(&json)) && json) {
                        const QString text = QString::fromWCharArray(json);
                        CoTaskMemFree(json);
                        Q_EMIT q->messageReceived(text);
                    }
                }),
            &messageToken);

        initializing = false;
        q->applyBounds();
        q->updateVisibility();
        debugLog(QStringLiteral("controller готов, страница: %1").arg(pageUrl));
        if (navigateWhenReady)
            navigate();
    }

    void navigate()
    {
        if (!webView) {
            navigateWhenReady = true;
            return;
        }
        navigateWhenReady = false;
        debugLog(QStringLiteral("navigate: %1").arg(pageUrl));
        webView->Navigate(wide(pageUrl));
    }

    void handleResourceRequest(ICoreWebView2WebResourceRequestedEventArgs *args)
    {
        if (!args || !environment)
            return;
        ICoreWebView2WebResourceRequest *request = nullptr;
        if (FAILED(args->get_Request(&request)) || !request)
            return;
        LPWSTR rawUri = nullptr;
        QString uri;
        if (SUCCEEDED(request->get_Uri(&rawUri)) && rawUri) {
            uri = QString::fromWCharArray(rawUri);
            CoTaskMemFree(rawUri);
        }
        request->Release();

        const QUrl url(uri);
        QString path = url.path();
        if (path.startsWith(QLatin1Char('/')))
            path.remove(0, 1);
        path = QUrl::fromPercentEncoding(path.toUtf8());
        if (path.isEmpty())
            path = QStringLiteral("index.html");

        QByteArray body;
        QString mime;
        bool found = false;
        if (provider)
            found = provider(path, &body, &mime);
        if (!found) {
            debugLog(QStringLiteral("404: %1").arg(path));
            qWarning() << "WebView2: ресурс не найден:" << path;
            body = QByteArrayLiteral("Not found");
            mime = QStringLiteral("text/plain; charset=utf-8");
        } else {
            debugLog(QStringLiteral("200: %1 (%2 байт)").arg(path).arg(body.size()));
        }

        const int status = found ? 200 : 404;
        const QString reason = found ? QStringLiteral("OK") : QStringLiteral("Not Found");
        if (mime.isEmpty())
            mime = QStringLiteral("application/octet-stream");
        const QString headers =
            QStringLiteral("Content-Type: %1\r\nCache-Control: no-store\r\n"
                           "Access-Control-Allow-Origin: *")
                .arg(mime);

        IStream *stream = SHCreateMemStream(
            reinterpret_cast<const BYTE *>(body.constData()),
            static_cast<UINT>(body.size()));
        if (!stream)
            return;
        ICoreWebView2WebResourceResponse *response = nullptr;
        const HRESULT created = environment->CreateWebResourceResponse(
            stream, status, wide(reason), wide(headers), &response);
        stream->Release();
        if (SUCCEEDED(created) && response) {
            args->put_Response(response);
            response->Release();
        }
    }

    void onNavigationCompleted(ICoreWebView2NavigationCompletedEventArgs *args)
    {
        BOOL success = FALSE;
        COREWEBVIEW2_WEB_ERROR_STATUS status = COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN;
        if (args) {
            args->get_IsSuccess(&success);
            args->get_WebErrorStatus(&status);
        }
        if (success) {
            debugLog(QStringLiteral("navigation completed: OK"));
            if (!ready) {
                ready = true;
                Q_EMIT q->ready();
            }
        } else {
            debugLog(QStringLiteral("navigation completed: ошибка %1")
                         .arg(static_cast<int>(status)));
            fail(QStringLiteral("Страница точного вида не загрузилась (код %1).")
                     .arg(static_cast<int>(status)));
        }
    }

    WebView2Host *q = nullptr;
    std::shared_ptr<bool> alive;
    WebView2Host::ResourceProvider provider;
    QString pageUrl = QStringLiteral("about:blank");
    bool navigateWhenReady = false;
    bool initializing = false;
    bool ready = false;

    HMODULE loader = nullptr;
    ICoreWebView2Environment *environment = nullptr;
    ICoreWebView2Controller *controller = nullptr;
    ICoreWebView2 *webView = nullptr;
    EventRegistrationToken resourceToken = {};
    EventRegistrationToken navigationToken = {};
    EventRegistrationToken newWindowToken = {};
    EventRegistrationToken messageToken = {};
};

WebView2Host::WebView2Host(QWidget *parent)
    : QWidget(parent)
    , m_impl(new Impl(this))
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors, false);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(80, 80);
}

WebView2Host::~WebView2Host()
{
    delete m_impl;
    m_impl = nullptr;
}

bool WebView2Host::isSupported(QString *reason)
{
    QString problem;
    if (loadCreateEnvironment(&problem)) {
        if (!runtimeVersionCached().isEmpty()) {
            if (reason)
                reason->clear();
            return true;
        }
        if (reason) {
            *reason = QStringLiteral(
                "Не найден Microsoft Edge WebView2 Runtime. Установите его "
                "(Evergreen Runtime) — точный вид формы требует этот компонент.");
        }
        return false;
    }
    if (reason)
        *reason = problem;
    return false;
}

QString WebView2Host::runtimeVersion()
{
    return runtimeVersionCached();
}

void WebView2Host::setResourceProvider(ResourceProvider provider)
{
    if (m_impl)
        m_impl->provider = std::move(provider);
}

void WebView2Host::setStartUrl(const QString &url)
{
    if (m_impl)
        m_impl->pageUrl = url;
}

void WebView2Host::load()
{
    if (!m_impl)
        return;
    if (isVisible()) {
        m_impl->initialize();
        if (m_impl->controller)
            m_impl->navigate();
    } else {
        m_impl->navigateWhenReady = true;
    }
}

void WebView2Host::reload()
{
    if (m_impl && m_impl->webView)
        m_impl->webView->Reload();
}

void WebView2Host::setZoomFactor(double factor)
{
    if (m_impl && m_impl->controller)
        m_impl->controller->put_ZoomFactor(factor);
}

void WebView2Host::openDevTools()
{
    if (m_impl && m_impl->webView)
        m_impl->webView->OpenDevToolsWindow();
}

void WebView2Host::applyBounds()
{
    if (!m_impl || !m_impl->controller)
        return;
    const qreal ratio = devicePixelRatioF();
    RECT bounds = {};
    bounds.right = static_cast<LONG>(std::lround(width() * ratio));
    bounds.bottom = static_cast<LONG>(std::lround(height() * ratio));
    m_impl->controller->put_Bounds(bounds);
}

void WebView2Host::updateVisibility()
{
    if (!m_impl || !m_impl->controller)
        return;
    m_impl->controller->put_IsVisible(isVisible() ? TRUE : FALSE);
}

void WebView2Host::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    applyBounds();
}

void WebView2Host::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_impl)
        return;
    if (!m_impl->controller)
        m_impl->initialize();
    updateVisibility();
    applyBounds();
    if (m_impl->controller && m_impl->navigateWhenReady)
        m_impl->navigate();
}

void WebView2Host::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    updateVisibility();
}

#else // Q_OS_WIN

class WebView2Host::Impl
{
public:
    explicit Impl(WebView2Host *host) { Q_UNUSED(host); }

    WebView2Host::ResourceProvider provider;
};

WebView2Host::WebView2Host(QWidget *parent)
    : QWidget(parent)
    , m_impl(new Impl(this))
{
}

WebView2Host::~WebView2Host()
{
    delete m_impl;
    m_impl = nullptr;
}

// События объявлены в заголовке всегда, поэтому заглушки нужны и без Windows:
// иначе таблица виртуальных функций остаётся с неразрешёнными символами.
void WebView2Host::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void WebView2Host::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
}

void WebView2Host::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
}

bool WebView2Host::isSupported(QString *reason)
{
    if (reason) {
        *reason = QStringLiteral(
            "Точный вид формы основан на Microsoft Edge WebView2 и доступен "
            "только в Windows.");
    }
    return false;
}

QString WebView2Host::runtimeVersion()
{
    return QString();
}

void WebView2Host::setResourceProvider(ResourceProvider provider)
{
    if (m_impl)
        m_impl->provider = std::move(provider);
}

void WebView2Host::setStartUrl(const QString &url) { Q_UNUSED(url); }
void WebView2Host::load() {}
void WebView2Host::reload() {}
void WebView2Host::setZoomFactor(double factor) { Q_UNUSED(factor); }
void WebView2Host::openDevTools() {}
void WebView2Host::applyBounds() {}
void WebView2Host::updateVisibility() {}

#endif // Q_OS_WIN

} // namespace v8reader
