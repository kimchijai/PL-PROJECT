#ifndef DISPLAYFORMATTING_H
#define DISPLAYFORMATTING_H

#include <QDateTime>
#include <QString>

#include <string>

inline QString guiFormatMoney(int cents) {
    return QStringLiteral("PHP %1")
        .arg(static_cast<double>(cents) / 100.0, 0, 'f', 2);
}

inline QString guiFormatTimestamp(const std::string& timestamp) {
    const QString text = QString::fromStdString(timestamp);
    const QDateTime parsed = QDateTime::fromString(text, Qt::ISODate);
    return parsed.isValid()
        ? parsed.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
        : text;
}

#endif
