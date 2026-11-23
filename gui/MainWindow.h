#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "PosController.h"

class QButtonGroup;
class QLabel;
class QStackedWidget;
class QWidget;
class QPushButton;
class QLineEdit;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(const std::string& databasePath = "bakery_pos.db",
                        QWidget* parent = nullptr);

    void navigateTo(int pageIndex);
    int currentPageIndex() const;

private:
    QWidget* createPlaceholderPage(int pageIndex, bool databaseReady,
                                   const std::string& databaseError,
                                   std::size_t productCount);
    void setSidebarCollapsed(bool collapsed);
    void backupDatabase();
    void restoreDatabase();
    QWidget* createLoginPage();
    QWidget* createAdministratorSetupPage();
    void submitLogin();
    void submitAdministratorSetup();
    void showAuthenticationPage();
    void refreshOperatorControls();
    void openOperatorManagement();
    void logout();

    PosController controller_;
    QButtonGroup* navigationGroup_;
    QStackedWidget* pageStack_;
    QStackedWidget* applicationStateStack_;
    QLabel* pageTitle_;
    QWidget* sidebar_;
    QPushButton* sidebarToggle_;
    QPushButton* operatorButton_;
    QPushButton* logoutButton_;
    QPushButton* manageOperatorsButton_;
    QWidget* loginPage_;
    QWidget* administratorSetupPage_;
    QWidget* applicationRoot_;
    bool sidebarCollapsed_;
    std::string databasePath_;
};

#endif
