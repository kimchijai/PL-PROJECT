/**
 * CartItem.h
 * ----------
 * Declares the CartItem class, which pairs a Product with a mutable
 * quantity for use inside a ShoppingCart.
 *
 * A CartItem stores a *copy* of the Product (id, name, price) rather
 * than a pointer. This keeps CartItem self-contained: it can be moved
 * around in a vector without worrying about dangling references to the
 * catalog, and its lifetime is completely independent of the catalog's.
 *
 * Quantity is mutable so the cashier can update or merge items without
 * creating a whole new CartItem object.
 *
 * Requirements: 3.5, 3.6, 5.3, 6.5
 */

#ifndef CARTITEM_H
#define CARTITEM_H

#include <string>
#include "Product.h"

class CartItem {
public:
    /**
     * Constructs a CartItem from a Product and an initial quantity.
     *
     * The Product is copied by value so this CartItem is self-contained
     * and independent of the original Product object in the catalog.
     *
     * @param product   The bakery product being purchased.
     * @param quantity  How many units are being ordered (must be > 0;
     *                  callers — specifically POS — are responsible for
     *                  validating this before constructing a CartItem).
     */
    CartItem(const Product& product, int quantity);

    // ----- Const accessors -----
    // These read-only methods let external code inspect the item without
    // being able to change it. They delegate directly to the stored
    // Product copy or return the stored quantity.

    /** Returns the product's unique identifier (e.g. "P001"). */
    std::string getProductId()   const;

    /** Returns the product's display name (e.g. "Chocolate Chip Cookie"). */
    std::string getProductName() const;

    /** Returns the unit price in PHP (e.g. 65.00). */
    double      getUnitPrice()   const;

    /** Returns the current quantity in the cart. */
    int         getQuantity()    const;

    /**
     * Returns the line-item subtotal: unit_price * quantity.
     *
     * The subtotal is computed on the fly every time this method is
     * called — it is NOT stored as a separate field. This guarantees
     * the subtotal is always consistent with the current quantity and
     * price; there is no risk of a stale cached value.
     */
    double      getSubtotal()    const;

    // ----- Mutators -----

    /**
     * Replaces the current quantity with newQty.
     *
     * Used by ShoppingCart::updateItem() when the cashier chooses to
     * set a new quantity for an existing cart item (menu option 5).
     *
     * @param newQty  The replacement quantity. POS validates that this
     *                is > 0 before calling this method.
     */
    void setQuantity(int newQty);

    /**
     * Adds additionalQty to the existing quantity.
     *
     * Used by ShoppingCart::addItem() when the cashier adds a product
     * that is already in the cart (menu option 2). Instead of creating
     * a duplicate line, the quantities are merged into this item.
     *
     * @param additionalQty  The number of extra units to add. POS
     *                       validates that this is > 0 before calling.
     */
    void addQuantity(int additionalQty);

private:
    Product product_;   // A value-copy of the catalog Product
    int     quantity_;  // Current quantity; mutable via setQuantity/addQuantity
};

#endif // CARTITEM_H
