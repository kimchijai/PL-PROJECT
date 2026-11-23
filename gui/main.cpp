#include "MainWindow.h"
#include "AppDataPaths.h"
#include "ApplicationFont.h"
#include "ProductImagePaths.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QStringList>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName("Crombi Cookies");
    QCoreApplication::setApplicationName("Crombi Bakery POS");

    QString fontError;
    if (!GuiApplicationFont::load(fontError)) {
        qCritical().noquote() << fontError;
        QMessageBox::critical(nullptr, "Font initialization failed", fontError);
        return 1;
    }
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
