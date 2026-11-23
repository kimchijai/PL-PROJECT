#ifndef PRODUCTREPOSITORY_H
#define PRODUCTREPOSITORY_H

#include "DatabaseManager.h"
#include "Product.h"
#include <string>
#include <vector>

struct ProductRecord {
    std::string productId;
    int categoryId;
    std::string name;
    int priceCents;
    bool active;
    std::string imagePath;
};

class ProductRepository {
public:
    explicit ProductRepository(DatabaseManager& database);

    bool create(const std::string& productId, int categoryId,
                const std::string& name, int priceCents, int initialStock,
                std::string& error);
    bool create(const std::string& productId, int categoryId,
                const std::string& name, int priceCents, int initialStock,
                const std::string& imagePath, std::string& error);
    bool findById(const std::string& productId, ProductRecord& record,
                  std::string& error) const;
    bool search(const std::string& term, std::vector<ProductRecord>& records,
                std::string& error) const;
    bool getActive(std::vector<ProductRecord>& records, std::string& error) const;
    bool update(const std::string& productId, int categoryId,
                const std::string& name, int priceCents, std::string& error);
    bool update(const std::string& productId, int categoryId,
                const std::string& name, int priceCents,
                const std::string& imagePath, std::string& error);
    bool deactivate(const std::string& productId, std::string& error);
    bool deleteProduct(const std::string& productId,
                       ProductRecord& deletedRecord,
                       bool& imagePathUnreferenced,
                       std::string& error,
                       const std::string& operatorName = "Unknown");

private:
    DatabaseManager& database_;

    static bool validateProduct(const std::string& productId,
                                int categoryId,
                                const std::string& name,
                                int priceCents,
                                int initialStock,
                                std::string& error);
};

#endif
