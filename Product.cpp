/**
 * Product.cpp
 * -----------
 * Defines the Product class declared in Product.h.
 *
 * The implementation is intentionally minimal: a constructor that
 * stores its arguments and three getters that return the stored values.
 * No business logic lives here — Product is a pure value type.
 *
 * Requirements: 1.1, 10.2
 */

#include "Product.h"

// --------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------

/**
 * Stores the product's ID, name, and price using member-initializer
 * list syntax, which is preferred in C++ because it initializes each
 * member directly rather than default-constructing it and then
 * assigning a value.
 */
Product::Product(std::string id, std::string name, double price)
    : id_(id), name_(name), price_(price) {}

// --------------------------------------------------------------------
// Const accessors
// --------------------------------------------------------------------

/**
 * Returns the product's unique identifier (e.g. "P001").
 * Marked const because calling this method never modifies the object.
 */
std::string Product::getId() const {
    return id_;
}

/**
 * Returns the product's display name (e.g. "Chocolate Chip Cookie").
 * Marked const for the same reason as getId().
 */
std::string Product::getName() const {
    return name_;
}

/**
 * Returns the product's unit price in PHP (e.g. 65.0).
 * The POS layer is responsible for formatting this as "PHP 65.00"
 * before displaying it — Product only stores the raw numeric value.
 * Marked const because reading price_ does not change the object.
 */
double Product::getPrice() const {
    return price_;
}
