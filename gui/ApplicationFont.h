#pragma once

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QStringList>

namespace GuiApplicationFont {
inline bool load(QString& error) {
    const QString fontPath = QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("fonts/GoogleSansFlex.ttf"));
    const int fontId = QFontDatabase::addApplicationFont(fontPath);
    if (fontId < 0) {
        error = QStringLiteral("Could not load the bundled Google Sans Flex font.");
        return false;
    }

    const QString family = QStringLiteral("Google Sans Flex");
    const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
    if (!families.contains(family)) {
        error = QStringLiteral(
            "The bundled font did not register the Google Sans Flex family.");
        return false;
    }

    QFont applicationFont = QApplication::font();
    applicationFont.setFamily(family);
    QApplication::setFont(applicationFont);
    return true;
}
}
