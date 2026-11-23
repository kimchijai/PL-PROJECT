#ifndef APPDATAPATHS_H
#define APPDATAPATHS_H

#include "../DatabaseManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QUuid>

#include <string>

namespace AppDataPaths {

inline bool copyProductUploads(const QString& dataDirectory,
                               const QStringList& assetRoots,
                               std::string& error) {
    const QString destinationDirectory =
        QDir(dataDirectory).filePath("assets/products/uploads");
    for (const QString& assetRoot : assetRoots) {
        const QString sourceDirectory =
            QDir(assetRoot).filePath("assets/products/uploads");
        QDir source(sourceDirectory);
        if (!source.exists())
            continue;
        if (!QDir().mkpath(destinationDirectory)) {
            error = "Could not create the product image data directory.";
            return false;
        }
        const QFileInfoList images = source.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo& image : images) {
            if (image.isSymLink())
                continue;
            const QString destination =
                QDir(destinationDirectory).filePath(image.fileName());
            if (QFileInfo::exists(destination))
                continue;
            if (!QFile::copy(image.absoluteFilePath(), destination)) {
                error = "Could not preserve uploaded product image: "
                    + image.fileName().toStdString();
                return false;
            }
        }
    }
    return true;
}

inline bool prepareDatabasePath(const QString& dataDirectory,
                                const QStringList& databaseSearchRoots,
                                const QStringList& assetSearchRoots,
                                QString& databasePath,
                                std::string& error) {
    if (dataDirectory.trimmed().isEmpty()) {
        error = "The application data directory is unavailable.";
        return false;
    }
    if (!QDir().mkpath(dataDirectory)) {
        error = "Could not create the application data directory: "
            + dataDirectory.toStdString();
        return false;
    }

    const QString destination =
        QDir(dataDirectory).filePath("bakery_pos.db");
    if (QFileInfo::exists(destination)) {
        if (!QFileInfo(destination).isFile()) {
            error = "The application database path is not a regular file.";
            return false;
        }
        if (!copyProductUploads(dataDirectory, assetSearchRoots, error))
            return false;
        databasePath = destination;
        return true;
    }

    QString legacyDatabase;
    for (const QString& root : databaseSearchRoots) {
        const QString candidate = QDir(root).filePath("bakery_pos.db");
        const QString normalizedCandidate =
            QDir::cleanPath(QFileInfo(candidate).absoluteFilePath());
        const QString normalizedDestination =
            QDir::cleanPath(QFileInfo(destination).absoluteFilePath());
#ifdef Q_OS_WIN
        const Qt::CaseSensitivity pathSensitivity = Qt::CaseInsensitive;
#else
        const Qt::CaseSensitivity pathSensitivity = Qt::CaseSensitive;
#endif
        if (QString::compare(normalizedCandidate, normalizedDestination,
                             pathSensitivity) == 0)
            continue;
        if (QFileInfo(candidate).isFile()) {
            legacyDatabase = candidate;
            break;
        }
    }

    if (legacyDatabase.isEmpty()) {
        databasePath = destination;
        return true;
    }

    const QString temporaryDatabase = QStringLiteral("%1.migrating-%2.db")
        .arg(destination, QUuid::createUuid().toString(QUuid::WithoutBraces));
    DatabaseManager source;
    if (!source.open(legacyDatabase.toStdString())) {
        error = "Could not open the existing database for migration: "
            + source.getLastError();
        return false;
    }
    if (!source.backupDatabase(temporaryDatabase.toStdString(), error)) {
        source.close();
        return false;
    }
    source.close();

    if (!copyProductUploads(dataDirectory, assetSearchRoots, error)) {
        QFile::remove(temporaryDatabase);
        return false;
    }
    if (!QFile::rename(temporaryDatabase, destination)) {
        QFile::remove(temporaryDatabase);
        if (QFileInfo::exists(destination)) {
            databasePath = destination;
            return true;
        }
        error = "Could not move the migrated database into the application data directory.";
        return false;
    }

    databasePath = destination;
    return true;
}

}

#endif
