#ifndef GUI_CSV_READER_HPP
#define GUI_CSV_READER_HPP

#include <QString>
#include <QStringList>

#include <QByteArray>

inline QString csvLineText(const QByteArray& bytes) {
    qsizetype length = bytes.size();
    while (length > 0 &&
           (bytes.at(length - 1) == '\n' || bytes.at(length - 1) == '\r')) {
        --length;
    }
    QString line = QString::fromUtf8(bytes.constData(), length);
    if (!line.isEmpty() && line.front() == QChar(0xFEFF)) line.remove(0, 1);
    return line;
}

inline bool parseCsvRow(const QString& line, QStringList& fields) {
    fields.clear();
    QString field;
    bool quoted = false;
    bool closedQuote = false;

    for (qsizetype index = 0; index < line.size(); ++index) {
        const QChar character = line.at(index);
        if (quoted) {
            if (character == QLatin1Char('"')) {
                if (index + 1 < line.size() &&
                    line.at(index + 1) == QLatin1Char('"')) {
                    field += character;
                    ++index;
                } else {
                    quoted = false;
                    closedQuote = true;
                }
            } else {
                field += character;
            }
        } else if (character == QLatin1Char(',')) {
            fields.push_back(field);
            field.clear();
            closedQuote = false;
        } else if (character == QLatin1Char('"')) {
            if (!field.isEmpty() || closedQuote) return false;
            quoted = true;
        } else {
            if (closedQuote && !character.isSpace()) return false;
            if (!closedQuote) field += character;
        }
    }

    if (quoted) return false;
    fields.push_back(field);
    return true;
}

#endif
