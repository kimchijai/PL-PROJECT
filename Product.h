/**
 * Product.h
 * ---------
 * Declares the Product class, which represents a single bakery item
 * in the Crombi Bakery POS catalog.
 *
 * A Product is immutable once constructed: it has no setters. The
 * catalog is built at startup and never changes during a session,
 * so read-only access is all that is ever needed.
 *
 * Requirements: 1.1, 10.2
 */

#ifndef PRODUCT_H
#define PRODUCT_H

#include <string>

class Product {
public:
    /**
     * Constructs a Product with the given ID, name, and price.
     *
     * @param id    Unique product identifier (e.g. "P001").
     * @param name  Human-readable product name (e.g. "Chocolate Chip Cookie").
     * @param price Unit price in PHP (e.g. 65.00).
     */
    Product(std::string id, std::string name, double price);

    // ----- Const accessors (no setters — products are immutable) -----

    /** Returns the product's unique identifier. */
    std::string getId()   const;

    /** Returns the product's display name. */
    std::string getName() const;

    /** Returns the product's unit price in PHP. */
    double getPrice()     const;

private:
    std::string id_;     // Unique identifier, e.g. "P001"
    std::string name_;   // Display name, e.g. "Chocolate Chip Cookie"
    double      price_;  // Unit price in PHP, e.g. 65.00
};

#endif // PRODUCT_H
