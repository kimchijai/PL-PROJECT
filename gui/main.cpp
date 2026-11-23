#include "MainWindow.h"
#include "AppDataPaths.h"
#include "ProductImagePaths.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMessageBox>
#include <QStringList>

namespace {
void loadPackagedFonts() {
    const QString fontsDirectory = QDir(QCoreApplication::applicationDirPath())
        .filePath("fonts");
    const QStringList fonts = {
        "Inter-Variable.ttf",
        "Poppins-Regular.ttf",
        "Poppins-SemiBold.ttf"
    };
    for (const QString& font : fonts) {
        const QString path = QDir(fontsDirectory).filePath(font);
        if (QFileInfo::exists(path)
            && QFontDatabase::addApplicationFont(path) < 0) {
            qWarning() << "Could not load packaged font:" << path;
        }
    }
}
}

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName("Crombi Cookies");
    QCoreApplication::setApplicationName("Crombi Bakery POS");

    loadPackagedFonts();
    const QString applicationDataDirectory =
        ProductImagePaths::applicationDataRoot();
    const QString expectedDatabase =
        QDir(applicationDataDirectory).filePath("bakery_pos.db");
    if (ProductImagePaths::usesPortableData()
        && (!QFileInfo(expectedDatabase).isFile()
            || QFileInfo(expectedDatabase).size() == 0)) {
        QMessageBox::critical(
            nullptr, "Portable database unavailable",
            "The portable database is missing or empty. Restore "
            "data/bakery_pos.db before starting the application.");
        return 1;
    }

    const QStringList databaseRoots = {
        QDir::currentPath(),
        QCoreApplication::applicationDirPath(),
        ProductImagePaths::assetRoot()
    };
    const QStringList assetRoots = ProductImagePaths::usesPortableData()
        ? QStringList{ProductImagePaths::assetRoot()}
        : QStringList{
            ProductImagePaths::assetRoot(),
            QDir::currentPath(),
            QCoreApplication::applicationDirPath()
        };
    QString databasePath;
    std::string error;
    if (!AppDataPaths::prepareDatabasePath(
            applicationDataDirectory, databaseRoots, assetRoots,
            databasePath, error)) {
        QMessageBox::critical(nullptr, "Database startup failed",
                              QString::fromStdString(error));
        return 1;
    }

    MainWindow window(databasePath.toStdString());
    window.show();
    return application.exec();
}
