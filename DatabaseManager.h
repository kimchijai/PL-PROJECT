#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <sqlite3.h>
#include <string>

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    bool open(const std::string& databasePath);
    void close();
    bool initializeSchema();
    bool seedBakeryProducts();

    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool backupDatabase(const std::string& destinationPath, std::string& error);
    bool validateDatabaseFile(const std::string& sourcePath, std::string& error) const;
    bool restoreDatabase(const std::string& sourcePath, std::string& error);
    bool getCurrentOperatorName(std::string& name, std::string& error);
    bool setCurrentOperatorName(const std::string& name, std::string& error);

    sqlite3* getHandle() const;
    std::string getLastError() const;

private:
    sqlite3* database_;
    std::string lastError_;

    bool execute(const std::string& sql);
    bool migrateInventoryLogProductReference();
};

#endif
