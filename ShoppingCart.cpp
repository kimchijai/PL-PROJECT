#include "ShoppingCart.h"

std::vector<CartItem>::iterator ShoppingCart::findItem(const std::string& productId) {
    for (auto it = items_.begin(); it != items_.end(); ++it) {
        if (it->getProductId() == productId) return it;
    }
    return items_.end();
}

void ShoppingCart::addItem(const Product& product, int quantity) {
    auto it = findItem(product.getId());
    if (it != items_.end()) {
        it->addQuantity(quantity);
    } else {
        items_.emplace_back(product, quantity);
    }
}

bool ShoppingCart::removeItem(const std::string& productId) {
    auto it = findItem(productId);
    if (it == items_.end()) return false;
    items_.erase(it);
    return true;
}

bool ShoppingCart::updateItem(const std::string& productId, int newQty) {
    auto it = findItem(productId);
    if (it == items_.end()) return false;
    it->setQuantity(newQty);
    return true;
}

void ShoppingCart::clear() {
    items_.clear();
}

bool ShoppingCart::isEmpty() const {
    return items_.empty();
}

bool ShoppingCart::containsProduct(const std::string& productId) const {
    for (const auto& item : items_) {
        if (item.getProductId() == productId) return true;
    }
    return false;
}

double ShoppingCart::getOrderTotal() const {
    double total = 0.0;
    for (const auto& item : items_) {
        total += item.getSubtotal();
    }
    return total;
}

const std::vector<CartItem>& ShoppingCart::getItems() const {
    return items_;
}
