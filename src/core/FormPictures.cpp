#include "core/FormPictures.h"

#include "core/Configuration.h"
#include "core/V8Container.h"
#include "core/V8Text.h"

namespace v8reader {
namespace {

// Коллекция «Общие картинки» (см. MetadataCatalog).
const char kCommonPicturesGuid[] = "7dcd43d9-aca5-4926-b549-1842e6a4e8cf";

// Поток данных картинки: первый строковый узел вида «#base64:…».
QString base64FromPictureData(const V8TextNode &node)
{
    if (node.isScalar()) {
        const QString value = node.scalar().trimmed();
        const int marker = value.indexOf(QStringLiteral("#base64:"));
        if (marker >= 0) {
            QString payload = value.mid(marker + 8);
            payload.remove(QLatin1Char('\r'));
            payload.remove(QLatin1Char('\n'));
            payload.remove(QLatin1Char(' '));
            return payload;
        }
    }
    for (const V8TextNode &child : node.children) {
        const QString data = base64FromPictureData(child);
        if (!data.isEmpty())
            return data;
    }
    return QString();
}

} // namespace

void FormPictureTable::loadFromConfiguration(const Configuration &configuration,
                                             const V8Container &container)
{
    m_container = &container;
    m_namesByGuid.clear();
    m_guidsByName.clear();
    m_dataByGuid.clear();
    for (const MetadataCategory &category : configuration.categories) {
        if (category.guid.compare(QLatin1String(kCommonPicturesGuid),
                                  Qt::CaseInsensitive) != 0) {
            continue;
        }
        for (const MetadataObject &object : category.objects) {
            const QString guid = object.guid.toLower();
            if (guid.isEmpty() || object.name.isEmpty())
                continue;
            m_namesByGuid.insert(guid, object.name);
            m_guidsByName.insert(object.name, guid);
        }
    }
}

QString FormPictureTable::reference(const QString &guid) const
{
    const QString key = guid.trimmed().toLower();
    const QString std = stdPictureName(key);
    if (!std.isEmpty())
        return QStringLiteral("StdPicture.") + std;
    const QString pictureName = name(key);
    if (pictureName.isEmpty())
        return QString();
    return QStringLiteral("CommonPicture.") + pictureName;
}

QString FormPictureTable::stdPictureName(const QString &guid)
{
    struct PlatformPicture
    {
        const char *guid;
        const char *name;
    };
    // Библиотека платформенных картинок: в контейнере они записаны GUID-ом,
    // а Form.xml называет их StdPicture.<Имя>. Сверено с Form.xml по корпусу
    // из 26 форм (пиктограммы «Сформировать», «Обратная связь», «Выполнено»,
    // «Информация», «Загрузить настройки отчёта», «Удалить»).
    static const PlatformPicture kPlatformPictures[] = {
        {"003024ed-fa25-42ac-9f53-f5014e383801", "ExecuteTask"},
        {"08a45a70-c221-4339-b3b1-9f11cb22147d", "Delete"},
        {"0ce78048-0196-4f80-a781-9829cdb7f43e", "GenerateReport"},
        {"283ecabd-aaed-41d1-ad46-6cca91c29120", "LoadReportSettings"},
        {"4b54770b-d069-4c0e-9b17-5cc2a01134d9", "Information"},
        {"78da2c47-172f-4d57-ab52-a06e40548136", "Message"},
        // Подменю «Создать» формы списка «Авансового отчёта»: в записи
        // картинки — этот GUID, в выгрузке — StdPicture.CreateListItem.
        {"977e831a-0e73-4d60-af51-091a6fa8612e", "CreateListItem"},
    };
    for (const PlatformPicture &picture : kPlatformPictures) {
        if (guid == QLatin1String(picture.guid))
            return QString::fromLatin1(picture.name);
    }
    return QString();
}

QString FormPictureTable::name(const QString &guid) const
{
    return m_namesByGuid.value(guid.trimmed().toLower());
}

QString FormPictureTable::dataByName(const QString &name) const
{
    return data(m_guidsByName.value(name));
}

// Имя стандартной картинки платформы по её коду из записи картинки: у
// стандартной картинки полезная нагрузка — одно отрицательное число.
// Соответствие кодов и имён снято с самого корпуса: имя берётся из Form.xml,
// код — из записи той же картинки в контейнере. Сверено по корпусу: −10 —
// CheckAll (кнопка «ПисьмаВыбратьВсе»), −11 — UncheckAll («ПисьмаИсключитьВсе»),
// −1 — InputFieldSelect («ВыбратьПериод»), −2 — InputFieldClear (пять
// декораций-крестиков закрытия баннера). Остальные коды платформы пока не
// встречались, поэтому картинка не отдаётся.
QString FormPictureTable::standardPictureName(int code)
{
    switch (code) {
    case -1:
        return QStringLiteral("StdPicture.InputFieldSelect");
    case -2:
        return QStringLiteral("StdPicture.InputFieldClear");
    case -10:
        return QStringLiteral("StdPicture.CheckAll");
    case -11:
        return QStringLiteral("StdPicture.UncheckAll");
    case -13:
        // Подменю «Печать» формы «АвансовыйОтчет/ФормаДокумента»: в записи
        // картинки стоит код −13, в выгрузке — StdPicture.Print.
        return QStringLiteral("StdPicture.Print");
    default:
        return QString();
    }
}

QString FormPictureTable::data(const QString &guid) const
{
    const QString key = guid.trimmed().toLower();
    auto cached = m_dataByGuid.constFind(key);
    if (cached != m_dataByGuid.constEnd())
        return cached.value();
    QString result;
    if (m_container) {
        const V8Element *element = m_container->find(key + QStringLiteral(".0"));
        if (!element)
            element = m_container->find(key);
        if (element) {
            QByteArray payload;
            if (m_container->elementPayload(*element, &payload, nullptr, nullptr)) {
                QString error;
                const QString text = V8Text::decode(payload, nullptr, &error);
                V8TextNode root;
                V8TextError parseError;
                if (!text.isEmpty() && V8Text::parse(text, root, &parseError))
                    result = base64FromPictureData(root);
            }
        }
    }
    m_dataByGuid.insert(key, result);
    return result;
}

} // namespace v8reader
