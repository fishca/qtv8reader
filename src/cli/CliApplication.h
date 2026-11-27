#ifndef QTV8READER_CLIAPPLICATION_H
#define QTV8READER_CLIAPPLICATION_H

#include <QString>

class QTextStream;

namespace v8reader {

class CliApplication
{
public:
    struct Options
    {
        QString command;
        QString path;
        QString argument;
        QString output;
        // Имя или синоним формы для команды form-model.
        QString form;
        // Имя модуля для команды view.
        QString module;
        // Имя макета для команды mxl.
        QString templateName;
        // Файл HTML для команды mxl; «-» — в stdout.
        QString htmlOutput;
        // Машиночитаемый вывод (команды view и mxl).
        bool json = false;
        bool raw = false;
        // Только дерево элементов формы (отладка разбора).
        bool items = false;
        // Узлы формы в порядке сериализации (отладка разбора).
        bool nodes = false;
    };

    int run(const Options &options, QTextStream &out, QTextStream &err);
};

} // namespace v8reader

#endif // QTV8READER_CLIAPPLICATION_H
