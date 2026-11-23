#ifndef OPERATORACCOUNTREPOSITORY_H
#define OPERATORACCOUNTREPOSITORY_H

#include "DatabaseManager.h"

#include <string>
#include <vector>

struct OperatorAccount {
    int accountId;
    std::string username;
    std::string role;
    bool active;
};

class OperatorAccountRepository {
public:
    explicit OperatorAccountRepository(DatabaseManager& database);

    bool hasAdministrator(bool& exists, std::string& error) const;
    bool createInitialAdministrator(const std::string& username,
                                    const std::string& password,
                                    OperatorAccount& account,
                                    std::string& error);
    bool authenticate(const std::string& username,
                      const std::string& password,
                      OperatorAccount& account,
                      std::string& error) const;
    bool getActiveAccount(int accountId, OperatorAccount& account,
                          std::string& error) const;
    bool listAccounts(std::vector<OperatorAccount>& accounts,
                      std::string& error) const;
    bool createAccount(const std::string& username,
                       const std::string& password,
                       const std::string& role,
                       OperatorAccount& account,
                       std::string& error);
    bool updateAccount(int accountId, const std::string& username,
                       const std::string& role, const std::string& newPassword,
                       std::string& error);
    bool deactivateAccount(int accountId, std::string& error);

    static bool validUsername(const std::string& username);
    static bool validPassword(const std::string& password);
    static bool validRole(const std::string& role);

private:
    DatabaseManager& database_;

    bool insertAccount(const std::string& username,
                       const std::string& password,
                       const std::string& role,
                       OperatorAccount& account,
                       std::string& error);
};

#endif
