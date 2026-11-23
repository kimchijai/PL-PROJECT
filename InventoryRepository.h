#ifndef INVENTORYREPOSITORY_H
#define INVENTORYREPOSITORY_H

#include "DatabaseManager.h"
#include <string>

struct InventoryLog {
    int logId;
    std::string productId;
    int quantityChange;
    std::string reason;
    int resultingQuantity;
    std::string loggedAt;
    std::string operatorName;
};

class InventoryRepository {
public:
    explicit InventoryRepository(DatabaseManager& database);

    bool getStock(const std::string& productId, int& quantity,
                  std::string& error) const;
    bool restock(const std::string& productId, int quantity,
                 const std::string& reason, std::string& error,
                 const std::string& operatorName = "Unknown");
    bool adjust(const std::string& productId, int quantityChange,
                const std::string& reason, std::string& error,
                const std::string& operatorName = "Unknown");
    bool getLatestLog(const std::string& productId, InventoryLog& log,
                      std::string& error) const;

private:
    DatabaseManager& database_;
};

#endif
