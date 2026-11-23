/**
 * ShoppingCart.h
 * --------------
 * Declares the ShoppingCart class, which manages the collection of
 * CartItem objects for the current transaction.
 *
 * ShoppingCart is the middle layer of the Model: it sits between
 * the catalog (read-only Products owned by POS) and the application
 * driver (POS) that validates all input before calling these methods.
 *
 * Design decisions:
 *  - Backing store is a std::vector<CartItem>. A vector gives O(n) lookup
 *    (fine for ≤5 distinct products) and keeps items in insertion order,
 *    which makes the receipt and cart display predictable.
 *  - The private findItem() iterator helper is the single point of
 *    truth for "is this product already in the cart?". addItem(),
 *    removeItem(), updateItem(), and containsProduct() all rely on it,
 *    so the lookup logic is defined once and cannot diverge.
 *  - Input validation (positive quantities, known product IDs) is the
 *    responsibility of POS, not ShoppingCart. ShoppingCart trusts that
 *    callers have already validated.
 *
 * Requirements: 3.5, 3.6, 5.3, 6.5, 10.4
 */

#ifndef SHOPPINGCART_H
#define SHOPPINGCART_H

#include <vector>
#include <string>
#include "CartItem.h"   // also pulls in Product.h transitively

class ShoppingCart {
public:
    // ----------------------------------------------------------------
    // Mutators
    // ----------------------------------------------------------------

    /**
     * Adds a product to the cart, or merges if it is already present.
     *
     * Uses findItem() to search for an existing CartItem with the same
     * Product_ID:
     *   - Found → calls CartItem::addQuantity(quantity) to accumulate
     *     (Requirement 3.6: duplicate items are merged, not duplicated).
     *   - Not found → constructs a new CartItem and appends it to items_
     *     (Requirement 3.5: a fresh line item is created).
     *
     * POS is responsible for ensuring product is a valid catalog entry
     * and quantity > 0 before calling this method.
     *
     * @param product   The catalog Product to add.
     * @param quantity  Number of units to add (caller guarantees > 0).
     */
    void addItem(const Product& product, int quantity);

    /**
     * Removes the CartItem with the given Product_ID from the cart.
     *
     * Uses findItem() to locate the item. If the product is not in the
     * cart the method returns false without modifying the cart.
     *
     * @param productId  The Product_ID string to remove (e.g. "P002").
     * @return true if the item was found and removed; false otherwise.
     * Requirements: 5.3
     */
    bool removeItem(const std::string& productId);

    /**
     * Sets the quantity of an existing CartItem to newQty.
     *
     * Uses findItem() to locate the item. If the product is not in the
     * cart the method returns false without modifying the cart.
     *
     * POS guarantees newQty > 0 before calling this method.
     *
     * @param productId  The Product_ID of the item to update.
     * @param newQty     The replacement quantity (caller guarantees > 0).
     * @return true if the item was found and updated; false otherwise.
     * Requirements: 6.5
     */
    bool updateItem(const std::string& productId, int newQty);

    /**
     * Removes all items from the cart.
     *
     * Called by POS::checkout() after a successful transaction so the
     * next order starts with an empty cart (Requirement 8.5).
     * Delegates directly to std::vector::clear().
     */
    void clear();

    // ----------------------------------------------------------------
    // Const queries
    // ----------------------------------------------------------------

    /**
     * Returns true if the cart contains no items.
     *
     * POS uses this to guard checkout (you cannot check out an empty
     * cart) and to decide whether to show "Cart is empty." in viewCart().
     */
    bool isEmpty() const;

    /**
     * Returns true if a CartItem with the given Product_ID is present.
     *
     * POS calls this before removeFromCart() and updateQuantity() to
     * give a friendly "product not in cart" error before attempting a
     * mutation.
     *
     * Implemented separately from findItem() because findItem() returns
     * a non-const iterator and cannot be called on a const ShoppingCart.
     * containsProduct() performs the same linear scan on the const items_.
     *
     * @param productId  The Product_ID to look up.
     * @return true if found; false otherwise.
     */
    bool containsProduct(const std::string& productId) const;

    /**
     * Returns the sum of all CartItem subtotals.
     *
     * Iterates the entire items_ vector and accumulates
     * CartItem::getSubtotal() (which is unit_price × quantity) for each
     * entry. The result is the Order_Total displayed in the receipt and
     * cart view.
     *
     * @return Total cost of all items in PHP.
     */
    double getOrderTotal() const;

    /**
     * Returns a const reference to the internal items vector.
     *
     * POS uses this in viewCart() and saved-receipt output to iterate over
     * all CartItems and format the display/receipt tables. Returning a
     * const reference avoids a copy while preventing external mutation.
     *
     * @return A const reference to items_.
     */
    const std::vector<CartItem>& getItems() const;

private:
    std::vector<CartItem> items_;   // The ordered list of cart line items

    /**
     * Returns an iterator to the first CartItem whose Product_ID matches
     * productId, or items_.end() if no match is found.
     *
     * This is the single lookup implementation used by addItem(),
     * removeItem(), and updateItem(). Centralising the search here
     * eliminates duplicated loop code and ensures all three methods
     * use identical comparison logic.
     *
     * Marked non-const because callers (addItem, removeItem, updateItem)
     * need a mutable iterator to modify or erase the located element.
     * containsProduct() performs its own const-compatible scan instead.
     *
     * @param productId  The Product_ID string to search for.
     * @return Mutable iterator to the matching CartItem, or items_.end().
     */
    std::vector<CartItem>::iterator findItem(const std::string& productId);
};

#endif // SHOPPINGCART_H
