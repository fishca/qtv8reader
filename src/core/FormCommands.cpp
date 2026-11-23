#include "core/FormCommands.h"

#include <iterator>

namespace v8reader {

namespace {

struct CommandName
{
    const char *guid;
    const char *name;
};

// Стандартные команды формы (CommandName = «Form.StandardCommand.<имя>»).
// Сверено с Form.xml: Create, Copy, CreateFolder, Write, WriteAndClose, Help,
// Change, NewWindow, Find, CancelSearch, CustomizeForm, SaveVariant,
// SaveReportSettings, LoadReportSettings.
const CommandName kFormCommands[] = {
    {"03df6ee5-883c-4cc6-b319-d886d1a9b2c8", "NewWindow"},
    {"198ea630-fda2-4cda-8a23-f999f4c67ee6", "CustomizeForm"},
    {"32df4349-2607-4c2b-a4b9-bca4a1a28bd7", "WriteAndClose"},
    {"342c531d-dc73-458a-8ac4-6a746916a33b", "Copy"},
    {"39bb0fe9-771d-4dd5-8a6e-2d16984523af", "Help"},
    {"4f834c38-add1-45e4-a9f3-cefe3efac5c9", "Create"},
    {"6886601d-276c-4d3f-af0a-05c586025608", "Change"},
    {"7910bb04-ddcc-4e5d-89f0-104c6ad0f187", "SaveReportSettings"},
    {"96e0bc70-f8ff-4732-8119-060923203629", "CancelSearch"},
    {"9bffcf73-7b1d-4a8d-bf23-5e051af3ee29", "SaveVariant"},
    {"b0c9afb6-320c-4e36-be21-8f6d48116415", "LoadReportSettings"},
    {"bdefa701-6685-453e-a02a-3683d0cc16d3", "Find"},
    {"d8772fd1-a3bf-417d-8334-c49968dbb45e", "CreateFolder"},
    {"fe558fde-99b3-45d0-a060-9fc2905309f6", "Write"},
};

// Стандартные команды элемента (CommandName =
// «Form.Item.<элемент>.StandardCommand.<имя>»): колонки и команды таблиц,
// поля форматированного документа, результата отчёта, просмотра PDF.
const CommandName kItemCommands[] = {
    {"01db2225-b62d-4112-a4b6-d39d627bf79f", "PageSetup"},
    {"0bdc43a3-79f0-48d6-bce4-1142542e1a59", "IncreaseFontSize"},
    {"0cf34151-92d3-42fd-954f-5938433908a4", "Find"},
    {"0e355e57-d603-4ac6-998b-c522c43d3668", "PrintImmediately"},
    {"17724105-6e59-4d52-8a42-cf0fb4838037", "BackColor"},
    {"1d13f9a3-402a-46cb-9c68-1709356840f2", "Preview"},
    {"1e5ebd8b-32ee-4af9-b85c-3a0417000660", "ScaleDown"},
    {"29fc1bfc-7bf8-4849-9b4b-3e42d12ebcbb", "FindNext"},
    {"2b5d0007-b74c-4786-a904-37e64bda8414", "NumberedList"},
    {"30f0b852-3284-4ae4-838b-f89b78388bdc", "RotateClockwise"},
    {"32a87619-85ce-495a-a195-2719f5e9c71e", "GoBack"},
    {"37740564-9e86-44a0-bea9-3f485a5a3f91", "MoveUp"},
    {"39f6b9f1-7aa1-4a03-a01b-e127d51bc228", "DecreaseIndent"},
    {"3e80231d-e169-456e-8373-0b9d4c15c8ab", "GoToEnd"},
    {"408f351e-0536-46be-8916-a891db9bfbe6", "LineSpacing"},
    {"44ad3ec9-f3c2-4913-9224-5f9fb6418743", "CancelSearch"},
    {"49602716-fea6-497f-8047-726404038857", "OutputList"},
    {"4a9ec98e-c814-47b6-911f-694effc10fc5", "ShowHeaders"},
    {"56ae90b6-588f-406e-919c-cc5cc7f86297", "AlignJustify"},
    {"5a331cec-bf93-4af5-8f51-80fd7118db47", "SaveAs"},
    {"5aa38159-2001-42ae-8451-f8cabe0762c3", "Preview"},
    {"5c52ec51-6000-4190-b8c8-bd6201a271f5", "Edit"},
    {"6d83186a-5838-40a5-95e7-8990193adf0a", "Hyperlink"},
    {"6f1ea963-0807-4de8-b544-b5666f500b05", "Redo"},
    {"71007f7d-1995-44aa-9125-9926e70a35b5", "TextColor"},
    {"7a294bdc-b86b-4b73-abc4-df9c811f61ef", "CopyToClipboard"},
    {"83670388-2e45-439e-9968-587eca6c7f8d", "CutToClipboard"},
    {"85a1b7ee-e94d-4783-b519-4af123f58596", "GoForward"},
    {"85bd789b-0047-46f9-9b2e-845907fc1b1d", "Underline"},
    {"871100d5-049d-4b22-a46a-fabf54bd64c3", "Char"},
    {"87ecfbdd-8e2b-4ba2-a315-0897020f382f", "AlignLeft"},
    {"8d772f97-c0ef-47c0-9cb0-efea28c61341", "Delete"},
    {"905692d2-c3e7-4433-8f10-8d2ce35f652b", "PasteFromClipboard"},
    {"9d8a3915-de52-4227-91cd-2fce22e09972", "Picture"},
    {"a0033f06-56f7-4855-b901-7ac66fe1bb99", "BulletedList"},
    {"a4e92b1d-5e86-4a86-b276-00e71ecf3fd4", "GoToBegin"},
    {"a8631f01-318a-4da2-80a9-9075c7524463", "Italic"},
    {"a8f6b59e-b712-4d3e-a974-55a3be4eb295", "Font"},
    {"ab0ebc39-68ee-4034-b2f4-43eee55bd651", "AlignCenter"},
    {"b0016a68-ec64-4e6d-b905-c71fd62efc4c", "Add"},
    {"b67f202a-dcf8-41f3-bda8-1ff9bed5f2ef", "SelectAll"},
    {"c0519548-2a9a-44de-a25e-faf01e089d4d", "Find"},
    {"d0a4d953-115b-4059-a6cb-6e67f903a4f3", "IncreaseIndent"},
    {"d9117be6-f436-40fd-9670-8d71b99b2477", "ScaleUp"},
    {"db1cd9b3-bdf4-43f5-abd6-c2e4bd85d709", "Strikeout"},
    {"e28575de-9fe8-4a8e-a285-13250de33d49", "RotateCounterclockwise"},
    {"e2d6f793-b786-4640-a91b-8d77f73860f1", "Print"},
    {"e428af27-c4f7-4577-b80e-95a79f94322d", "AlignRight"},
    {"ec647dcc-2be7-486c-9046-d8b371f9909e", "DecreaseFontSize"},
    {"f20eefc2-f819-4ab1-be67-87b3ca2e26e6", "Bold"},
    {"f5814962-2bef-43dd-b633-a193d4b0970e", "Undo"},
    {"fa51b106-eae6-44c7-8054-76cbb3100603", "MoveDown"},
};

QString lookup(const CommandName *table, int count, const QString &guid)
{
    if (guid.isEmpty())
        return QString();
    const QString key = guid.toLower();
    for (int i = 0; i < count; ++i) {
        if (key == QLatin1String(table[i].guid))
            return QString::fromLatin1(table[i].name);
    }
    return QString();
}

} // namespace

QString formStandardCommandName(const QString &guid)
{
    return lookup(kFormCommands, int(std::size(kFormCommands)), guid);
}

QString itemStandardCommandName(const QString &guid)
{
    return lookup(kItemCommands, int(std::size(kItemCommands)), guid);
}

} // namespace v8reader
