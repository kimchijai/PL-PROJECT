/**
 * CartItem.cpp
 * ------------
 * Defines the CartItem class declared in CartItem.h.
 *
 * CartItem is a thin wrapper around a Product copy plus a quantity.
 * Almost every method is a one-liner — the complexity lives in
 * ShoppingCart (which decides when to create, merge, or remove items)
 * and in POS (which validates all input before touching the cart).
 *
 * Requirements: 3.5, 3.6, 5.3, 6.5
 */

#include "CartItem.h"

// --------------------------------------------------------------------
// Constructor
// --------------------------------------------------------------------

/**
 * Copies the Product by value and stores the initial quantity.
 *
 * Using a member-initializer list is the preferred C++ style: each
 * member is constructed directly with the given value instead of being
 * default-constructed first and then assigned.
 *
 * The Product copy means this CartItem is completely self-contained.
 * Even if the catalog were somehow modified (it isn't in this system),
 * this item would be unaffected.
 */
CartItem::CartItem(const Product& product, int quantity)
    : product_(product), quantity_(quantity) {}

// --------------------------------------------------------------------
// Const accessors — delegate to the stored Product or return quantity_
// --------------------------------------------------------------------

/**
 * Returns the product's unique identifier (e.g. "P001").
 * Delegates to the stored Product copy.
 */
std::string CartItem::getProductId() const {
    return product_.getId();
}

/**
 * Returns the product's display name (e.g. "Chocolate Chip Cookie").
 * Delegates to the stored Product copy.
 */
std::string CartItem::getProductName() const {
    return product_.getName();
}

/**
 * Returns the unit price in PHP (e.g. 65.00).
 * Delegates to the stored Product copy.
 */
double CartItem::getUnitPrice() const {
    return product_.getPrice();
}

/**
 * Returns how many units of this product are currently in the cart.
 */
int CartItem::getQuantity() const {
    return quantity_;
}

/**
 * Computes and returns the line-item subtotal: unit_price * quantity.
 *
 * Key design decision: the subtotal is computed on the fly rather than
 * cached in a member variable. This avoids an entire class of bugs
 * where the cached value could become stale after a quantity update.
 * Since both inputs (price and quantity) are always in sync, the result
 * is always correct.
 *
 * Example: 3 Chocolate Chip Cookies at PHP 65.00 → PHP 195.00.
 */
double CartItem::getSubtotal() const {
    return product_.getPrice() * quantity_;
}

// --------------------------------------------------------------------
// Mutators — called by ShoppingCart after POS has validated the input
// --------------------------------------------------------------------

/**
 * Replaces the stored quantity with newQty.
 *
 * Called when the cashier updates an existing item's quantity (menu
 * option 5 → ShoppingCart::updateItem → this method).
 *
 * POS has already confirmed newQty > 0 before this point, so no
 * additional validation is needed here.
 *
 * Example: quantity was 3; cashier sets it to 5 → quantity_ becomes 5.
 */
void CartItem::setQuantity(int newQty) {
    quantity_ = newQty;
}

/**
 * Adds additionalQty to the current quantity.
 *
 * Called when the cashier adds a product that is already in the cart
 * (menu option 2 → ShoppingCart::addItem → this method). The
 * quantities are accumulated instead of creating a duplicate line item.
 *
 * POS has already confirmed additionalQty > 0 before this point.
 *
 * Example: quantity was 2; cashier adds 3 more → quantity_ becomes 5.
 */
void CartItem::addQuantity(int additionalQty) {
    quantity_ += additionalQty;
}
