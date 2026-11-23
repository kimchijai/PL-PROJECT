#include "OperatorManagementDialog.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

OperatorManagementDialog::OperatorManagementDialog(
    PosController& controller, QWidget* parent)
    : QDialog(parent),
      controller_(controller),
      accountsTable_(new QTableWidget(this)),
      usernameInput_(new QLineEdit(this)),
      passwordInput_(new QLineEdit(this)),
      confirmPasswordInput_(new QLineEdit(this)),
      roleInput_(new QComboBox(this)),
      messageLabel_(new QLabel(this)),
      saveButton_(new QPushButton("Create account", this)),
      deactivateButton_(new QPushButton("Deactivate account", this)),
      selectedAccountId_(0) {
    setWindowTitle("Manage operator accounts");
    resize(680, 520);

    auto* layout = new QVBoxLayout(this);
    accountsTable_->setObjectName("operatorAccountsTable");
    accountsTable_->setColumnCount(3);
    accountsTable_->setHorizontalHeaderLabels({"Username", "Role", "Status"});
    accountsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    accountsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    accountsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    accountsTable_->verticalHeader()->hide();
    QFont headerFont = accountsTable_->horizontalHeader()->font();
    headerFont.setBold(true);
    accountsTable_->horizontalHeader()->setFont(headerFont);
    accountsTable_->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    layout->addWidget(accountsTable_, 1);

    auto* form = new QFormLayout();
    usernameInput_->setObjectName("operatorUsernameInput");
    passwordInput_->setObjectName("operatorPasswordInput");
    confirmPasswordInput_->setObjectName("operatorConfirmPasswordInput");
    passwordInput_->setEchoMode(QLineEdit::Password);
    confirmPasswordInput_->setEchoMode(QLineEdit::Password);
    passwordInput_->setMaxLength(1024);
    confirmPasswordInput_->setMaxLength(1024);
    passwordInput_->setPlaceholderText(
        "Required for new accounts; leave blank to keep current");
    roleInput_->setObjectName("operatorRoleInput");
    roleInput_->addItem("Operator", "operator");
    roleInput_->addItem("Administrator", "administrator");
    form->addRow("Username", usernameInput_);
    form->addRow("Role", roleInput_);
    form->addRow("New password", passwordInput_);
    form->addRow("Confirm password", confirmPasswordInput_);
    layout->addLayout(form);

    messageLabel_->setObjectName("operatorAccountMessage");
    messageLabel_->setWordWrap(true);
    layout->addWidget(messageLabel_);

    auto* buttons = new QHBoxLayout();
    auto* newButton = new QPushButton("New account", this);
    newButton->setObjectName("newOperatorAccountButton");
    saveButton_->setObjectName("saveOperatorAccountButton");
    deactivateButton_->setObjectName("deactivateOperatorAccountButton");
    auto* closeButton = new QPushButton("Close", this);
    buttons->addWidget(newButton);
    buttons->addStretch();
    buttons->addWidget(deactivateButton_);
    buttons->addWidget(saveButton_);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(accountsTable_, &QTableWidget::currentCellChanged,
            this, [this](int, int, int, int) { loadSelectedAccount(); });
    connect(newButton, &QPushButton::clicked,
            this, [this]() { clearSelection(); });
    connect(saveButton_, &QPushButton::clicked,
            this, [this]() { saveAccount(); });
    connect(deactivateButton_, &QPushButton::clicked,
            this, [this]() { deactivateAccount(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    clearSelection();
    refreshAccounts();
}

void OperatorManagementDialog::refreshAccounts() {
    std::vector<OperatorAccount> accounts;
    std::string error;
    if (!controller_.listOperatorAccounts(accounts, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }

    accountsTable_->setRowCount(static_cast<int>(accounts.size()));
    for (int row = 0; row < static_cast<int>(accounts.size()); ++row) {
        const auto& account = accounts[static_cast<std::size_t>(row)];
        auto* username = new QTableWidgetItem(
            QString::fromStdString(account.username));
        username->setData(Qt::UserRole, account.accountId);
        accountsTable_->setItem(row, 0, username);
        accountsTable_->setItem(
            row, 1, new QTableWidgetItem(
                account.role == "administrator" ? "Administrator" : "Operator"));
        accountsTable_->setItem(
            row, 2, new QTableWidgetItem(account.active ? "Active" : "Inactive"));
    }
}

void OperatorManagementDialog::loadSelectedAccount() {
    const int row = accountsTable_->currentRow();
    if (row < 0 || !accountsTable_->item(row, 0)) {
        clearSelection();
        return;
    }
    selectedAccountId_ = accountsTable_->item(row, 0)
        ->data(Qt::UserRole).toInt();
    usernameInput_->setText(accountsTable_->item(row, 0)->text());
    roleInput_->setCurrentIndex(
        accountsTable_->item(row, 1)->text() == "Administrator" ? 1 : 0);
    passwordInput_->clear();
    confirmPasswordInput_->clear();
    saveButton_->setText("Save changes");
    const bool active = accountsTable_->item(row, 2)->text() == "Active";
    deactivateButton_->setEnabled(
        active && selectedAccountId_ != controller_.currentOperatorAccountId());
    setMessage(active ? "Leave both password fields blank to keep the current password."
                      : "This account is inactive and cannot be edited.",
               false);
    usernameInput_->setEnabled(active);
    roleInput_->setEnabled(active);
    passwordInput_->setEnabled(active);
    confirmPasswordInput_->setEnabled(active);
    saveButton_->setEnabled(active);
}

void OperatorManagementDialog::clearSelection() {
    selectedAccountId_ = 0;
    accountsTable_->clearSelection();
    usernameInput_->clear();
    passwordInput_->clear();
    confirmPasswordInput_->clear();
    usernameInput_->setEnabled(true);
    roleInput_->setEnabled(true);
    passwordInput_->setEnabled(true);
    confirmPasswordInput_->setEnabled(true);
    saveButton_->setEnabled(true);
    saveButton_->setText("Create account");
    deactivateButton_->setEnabled(false);
    setMessage("Usernames are 3-32 letters, numbers, '.', '_' or '-'. "
               "Passwords must contain at least 12 characters.", false);
}

void OperatorManagementDialog::saveAccount() {
    const std::string username = usernameInput_->text().trimmed().toStdString();
    const std::string role = roleInput_->currentData().toString().toStdString();
    const std::string password = passwordInput_->text().toStdString();
    const std::string confirmation =
        confirmPasswordInput_->text().toStdString();
    if (password != confirmation) {
        setMessage("Password confirmation does not match.", true);
        return;
    }

    std::string error;
    const bool saved = selectedAccountId_ == 0
        ? controller_.createOperatorAccount(username, password, role, error)
        : controller_.updateOperatorAccount(
              selectedAccountId_, username, role, password, error);
    if (!saved) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    refreshAccounts();
    clearSelection();
    setMessage("Operator account saved.", false);
}

void OperatorManagementDialog::deactivateAccount() {
    if (selectedAccountId_ == 0)
        return;
    const int confirmation = QMessageBox::question(
        this, "Deactivate operator account",
        "Deactivate this account? Historical transactions and inventory "
        "records will remain unchanged.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (confirmation != QMessageBox::Yes)
        return;

    std::string error;
    if (!controller_.deactivateOperatorAccount(selectedAccountId_, error)) {
        setMessage(QString::fromStdString(error), true);
        return;
    }
    refreshAccounts();
    clearSelection();
    setMessage("Operator account deactivated.", false);
}

void OperatorManagementDialog::setMessage(const QString& message,
                                          bool isError) {
    messageLabel_->setText(message);
    messageLabel_->setStyleSheet(isError ? "color: #A12626;" : "color: #59434A;");
}
