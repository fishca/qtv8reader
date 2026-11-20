#ifndef QTV8READER_WEBVIEW2HOST_H
#define QTV8READER_WEBVIEW2HOST_H

#include <QByteArray>
#include <QString>
#include <QWidget>

#include <functional>

namespace v8reader {

// Гибридный элемент управления Microsoft Edge WebView2 (Chromium).
//
// Виджет показывает локальную страницу, содержимое которой целиком отдаёт
// приложение: все запросы к https://qv8.local/ перехватываются и
// обслуживаются провайдером ресурсов, без выхода в сеть. Так «точный вид»
// формы (HTML/CSS/JS рендерер) получает данные прямо из контейнера 1С.
//
// Если среда WebView2 недоступна (нет WebView2Loader.dll или не установлен
// сам браузер), isSupported() вернёт false и виджет покажет поясняющий текст.
class WebView2Host : public QWidget
{
    Q_OBJECT

public:
    // Отдаёт содержимое виртуального файла по пути относительно корня
    // виртуального хоста (без ведущего слэша). Возвращает false, если файла
    // нет — тогда страница получит 404.
    using ResourceProvider =
        std::function<bool(const QString &path, QByteArray *data, QString *mime)>;

    explicit WebView2Host(QWidget *parent = nullptr);
    ~WebView2Host() override;

    // Доступна ли среда WebView2. Причина недоступности пишется в reason.
    static bool isSupported(QString *reason = nullptr);
    // Версия установленного WebView2 Runtime (пусто, если неизвестна).
    static QString runtimeVersion();

    void setResourceProvider(ResourceProvider provider);
    // Адрес стартовой страницы (например,
    // https://qv8.local/formpreview/index.html?form=Documents/...).
    void setStartUrl(const QString &url);
    void load();
    void reload();
    void setZoomFactor(double factor);
    void openDevTools();

signals:
    void ready();
    void loadFailed(const QString &reason);
    void messageReceived(const QString &json);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void initialize();
    void applyBounds();
    void updateVisibility();

    class Impl;
    Impl *m_impl = nullptr;
};

} // namespace v8reader

#endif // QTV8READER_WEBVIEW2HOST_H
