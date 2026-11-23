#ifndef PRODUCTIMAGEPATHS_H
#define PRODUCTIMAGEPATHS_H

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QStandardPaths>

namespace ProductImagePaths {

inline QString assetRoot() {
    const QString applicationDirectory = QCoreApplication::applicationDirPath();
    const auto findRoot = [](const QString& start) {
        QDir directory(start);
        for (;;) {
            if (directory.exists("assets/products"))
                return directory.absolutePath();
            if (!directory.cdUp())
                break;
        }
        return QString();
    };

    QString root = findRoot(applicationDirectory);
    if (root.isEmpty())
        root = findRoot(QDir::currentPath());
    if (!root.isEmpty())
        return root;
    return QDir(applicationDirectory).absolutePath();
}

inline QString portableDataRoot(const QString& applicationDirectory) {
    const QString marker =
        QDir(applicationDirectory).filePath("portable.ini");
    if (!QFileInfo(marker).isFile())
        return QString();
    return QDir(applicationDirectory).filePath("data");
}

inline bool usesPortableData(const QString& applicationDirectory) {
    return !portableDataRoot(applicationDirectory).isEmpty();
}

inline bool usesPortableData() {
    return usesPortableData(QCoreApplication::applicationDirPath());
}

inline QString embeddedProductImage(const QString& productId) {
    struct ProductImage {
        const char* productId;
        const char* resourcePath;
    };
    static const ProductImage images[] = {
        {"P001", ":/assets/products/chocolate_chip.jpg"},
        {"P002", ":/assets/products/double_chocolate.jpg"},
        {"P003", ":/assets/products/red_velvet.jpg"},
        {"P004", ":/assets/products/smores.jpg"},
        {"P005", ":/assets/products/brookies.jpg"},
        {"P006", ":/assets/products/nutella.jpg"},
        {"P007", ":/assets/products/dubai_chewy_one_piece.jpg"},
        {"P008", ":/assets/products/dubai_chewy_four_pieces.jpg"}
    };
    for (const auto& image : images) {
        if (productId == QLatin1String(image.productId))
            return QLatin1String(image.resourcePath);
    }
    return QString();
}

inline QString applicationDataRoot(const QString& applicationDirectory,
                                   const QString& standardDataLocation) {
    const QString portableRoot = portableDataRoot(applicationDirectory);
    if (!portableRoot.isEmpty())
        return portableRoot;
    return standardDataLocation;
}

inline QString applicationDataRoot() {
    const QString applicationDirectory =
        QCoreApplication::applicationDirPath();
    const QString standardDataLocation = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    return applicationDataRoot(applicationDirectory, standardDataLocation);
}

inline QString uploadDirectory() {
    return QDir(applicationDataRoot()).filePath(
        "assets/products/uploads");
}

inline QString resolve(const QString& path, const QString& dataRoot,
                       const QString& fallbackAssetRoot) {
    const QFileInfo supplied(path);
    if (supplied.isAbsolute())
        return path;

    const QString normalizedPath = QDir::fromNativeSeparators(path);
    const QString uploadPrefix = "assets/products/uploads/";
    if (normalizedPath.startsWith(uploadPrefix, Qt::CaseInsensitive)) {
        const QString dataPath = QDir(dataRoot).filePath(normalizedPath);
        if (QFileInfo::exists(dataPath))
            return dataPath;
    }
    return QDir(fallbackAssetRoot).filePath(normalizedPath);
}

inline QString resolve(const QString& path) {
    return resolve(path, applicationDataRoot(), assetRoot());
}

inline QPixmap loadProductImage(const QString& productId,
                                const QString& imagePath) {
    QPixmap image;
    if (!imagePath.isEmpty())
        image.load(resolve(imagePath));
    if (image.isNull()) {
        const QString resource = embeddedProductImage(productId);
        if (!resource.isEmpty())
            image.load(resource);
    }
    return image;
}

inline bool removeUnreferencedUpload(const QString& path, QString& error) {
    const QString normalizedPath = QDir::fromNativeSeparators(path);
    const QString uploadPrefix = "assets/products/uploads/";
    if (!normalizedPath.startsWith(uploadPrefix, Qt::CaseInsensitive))
        return true;

    const QFileInfo image(resolve(normalizedPath));
    if (!image.exists())
        return true;
    if (!image.isFile() || image.isSymLink()) {
        error = "The product image is not a regular upload file.";
        return false;
    }

    const QString imageDirectory =
        QFileInfo(image.canonicalFilePath()).path();
    const QString appUploadsDirectory =
        QDir(uploadDirectory()).canonicalPath();
    const QString legacyUploadsDirectory =
        QDir(QDir(assetRoot()).filePath("assets/products/uploads"))
            .canonicalPath();
    const QString imagePath = image.canonicalFilePath();
    const bool isAppUpload = !appUploadsDirectory.isEmpty()
        && imageDirectory.compare(appUploadsDirectory,
                                  Qt::CaseInsensitive) == 0;
    const bool isLegacyUpload = !legacyUploadsDirectory.isEmpty()
        && imageDirectory.compare(legacyUploadsDirectory,
                                  Qt::CaseInsensitive) == 0;
    if (imagePath.isEmpty() || (!isAppUpload && !isLegacyUpload)) {
        error = "The product image is outside the uploads directory.";
        return false;
    }
    if (!QFile::remove(imagePath)) {
        error = "Could not remove the unused product image file.";
        return false;
    }
    return true;
}

}

#endif
