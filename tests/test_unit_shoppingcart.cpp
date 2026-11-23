/**
 * test_unit_shoppingcart.cpp
 * --------------------------
 * Unit tests for the ShoppingCart class.
 *
 * Validates: Requirements 3.5, 3.6, 4.2, 5.3, 6.5
 *
 * Coverage:
 *   - isEmpty()        : fresh cart is empty; non-empty cart is not
 *   - addItem()        : new item is inserted (Req 3.5)
 *                        duplicate item is merged, not duplicated (Req 3.6)
 *   - removeItem()     : removes an existing item; returns false for absent ID (Req 5.3)
 *   - updateItem()     : sets quantity on an existing item; returns false for absent ID (Req 6.5)
 *   - getOrderTotal()  : sums subtotals correctly with known values (Req 4.2)
 *   - clear()          : empties the cart
 */

#include "doctest.h"

#include "Product.h"
#include "CartItem.h"
#include "ShoppingCart.h"

// ---------------------------------------------------------------------------
// Catalog helpers — five real products from the Crombi Bakery catalog
// ---------------------------------------------------------------------------

static Product p001() { return Product("P001", "Chocolate Chip Cookie",  65.00); }
static Product p002() { return Product("P002", "Double Chocolate Cookie", 70.00); }
static Product p003() { return Product("P003", "Red Velvet Cookie",       70.00); }
static Product p004() { return Product("P004", "S'mores Cookie",          75.00); }
static Product p005() { return Product("P005", "Brookies",                75.00); }

// ---------------------------------------------------------------------------
// isEmpty()
// ---------------------------------------------------------------------------

TEST_SUITE("ShoppingCart::isEmpty") {

    TEST_CASE("fresh cart is empty") {
        ShoppingCart cart;
        CHECK(cart.isEmpty() == true);
    }

    TEST_CASE("cart is not empty after adding an item") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);
        CHECK(cart.isEmpty() == false);
    }

    TEST_CASE("cart is empty again after removing the only item") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);
        cart.removeItem("P001");
        CHECK(cart.isEmpty() == true);
    }
}

// ---------------------------------------------------------------------------
// addItem() — Requirement 3.5 (new item) and 3.6 (merge duplicate)
// ---------------------------------------------------------------------------

TEST_SUITE("ShoppingCart::addItem") {

    TEST_CASE("adding a new item creates one line in the cart — Req 3.5") {
        ShoppingCart cart;
        cart.addItem(p001(), 2);

        REQUIRE(cart.getItems().size() == 1);
        CHECK(cart.getItems()[0].getProductId()  == "P001");
        CHECK(cart.getItems()[0].getQuantity()   == 2);
    }

    TEST_CASE("adding two different products creates two lines") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);
        cart.addItem(p002(), 3);

        CHECK(cart.getItems().size() == 2);
    }

    TEST_CASE("adding the same product twice merges quantities — Req 3.6") {
        ShoppingCart cart;
        cart.addItem(p002(), 2);
        cart.addItem(p002(), 3);   // same product, should merge

        // Still only one line item
        REQUIRE(cart.getItems().size() == 1);
        // Quantity should be 2 + 3 = 5
        CHECK(cart.getItems()[0].getQuantity() == 5);
    }

    TEST_CASE("merged item subtotal reflects combined quantity — Req 3.6") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);   // PHP 65.00
        cart.addItem(p001(), 4);   // merge: total qty 5 × PHP 65.00 = PHP 325.00

        CHECK(cart.getItems()[0].getSubtotal() == doctest::Approx(325.00));
    }

    TEST_CASE("adding same product three times accumulates all quantities") {
        ShoppingCart cart;
        cart.addItem(p003(), 1);
        cart.addItem(p003(), 2);
        cart.addItem(p003(), 3);

        REQUIRE(cart.getItems().size() == 1);
        CHECK(cart.getItems()[0].getQuantity() == 6);
    }
}

// ---------------------------------------------------------------------------
// removeItem() — Requirement 5.3
// ---------------------------------------------------------------------------

TEST_SUITE("ShoppingCart::removeItem") {

    TEST_CASE("removes a present item and returns true") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);
        bool removed = cart.removeItem("P001");

        CHECK(removed == true);
        CHECK(cart.isEmpty() == true);
    }

    TEST_CASE("returns false for an ID not in the cart") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);
        bool removed = cart.removeItem("P999");   // absent ID

        CHECK(removed == false);
        // Cart unchanged
        CHECK(cart.getItems().size() == 1);
    }

    TEST_CASE("returns false on an empty cart") {
        ShoppingCart cart;
        CHECK(cart.removeItem("P001") == false);
    }

    TEST_CASE("removes the correct item when multiple items are present") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);
        cart.addItem(p002(), 2);
        cart.addItem(p003(), 3);

        cart.removeItem("P002");

        REQUIRE(cart.getItems().size() == 2);
        // P001 and P003 should remain; P002 should be gone
        CHECK(cart.containsProduct("P001") == true);
        CHECK(cart.containsProduct("P002") == false);
        CHECK(cart.containsProduct("P003") == true);
    }
}

// ---------------------------------------------------------------------------
// updateItem() — Requirement 6.5
// ---------------------------------------------------------------------------

TEST_SUITE("ShoppingCart::updateItem") {

    TEST_CASE("updates quantity of a present item and returns true") {
        ShoppingCart cart;
        cart.addItem(p004(), 1);
        bool updated = cart.updateItem("P004", 5);

        CHECK(updated == true);
        CHECK(cart.getItems()[0].getQuantity() == 5);
    }

    TEST_CASE("returns false for an ID not in the cart") {
        ShoppingCart cart;
        cart.addItem(p004(), 1);
        bool updated = cart.updateItem("P999", 5);   // absent ID

        CHECK(updated == false);
        // Original item is unchanged
        CHECK(cart.getItems()[0].getQuantity() == 1);
    }

    TEST_CASE("returns false on an empty cart") {
        ShoppingCart cart;
        CHECK(cart.updateItem("P001", 3) == false);
    }

    TEST_CASE("updated subtotal reflects new quantity") {
        ShoppingCart cart;
        cart.addItem(p005(), 1);       // PHP 75.00
        cart.updateItem("P005", 4);    // 4 × PHP 75.00 = PHP 300.00

        CHECK(cart.getItems()[0].getSubtotal() == doctest::Approx(300.00));
    }

    TEST_CASE("update replaces quantity, does not accumulate") {
        ShoppingCart cart;
        cart.addItem(p001(), 3);
        cart.updateItem("P001", 7);
        cart.updateItem("P001", 2);    // final value should be 2, not 3+7+2

        CHECK(cart.getItems()[0].getQuantity() == 2);
    }
}

// ---------------------------------------------------------------------------
// getOrderTotal() — Requirement 4.2
// ---------------------------------------------------------------------------

TEST_SUITE("ShoppingCart::getOrderTotal") {

    TEST_CASE("total is zero on an empty cart") {
        ShoppingCart cart;
        CHECK(cart.getOrderTotal() == doctest::Approx(0.00));
    }

    TEST_CASE("total equals single item subtotal") {
        ShoppingCart cart;
        cart.addItem(p001(), 3);   // 3 × PHP 65.00 = PHP 195.00
        CHECK(cart.getOrderTotal() == doctest::Approx(195.00));
    }

    TEST_CASE("total sums all items correctly with known values") {
        ShoppingCart cart;
        cart.addItem(p001(), 2);   // 2 × 65.00 = 130.00
        cart.addItem(p002(), 1);   // 1 × 70.00 =  70.00
        cart.addItem(p004(), 3);   // 3 × 75.00 = 225.00
        // Expected total: 130.00 + 70.00 + 225.00 = 425.00

        CHECK(cart.getOrderTotal() == doctest::Approx(425.00));
    }

    TEST_CASE("total updates correctly after adding more of the same product") {
        ShoppingCart cart;
        cart.addItem(p003(), 2);   // 2 × 70.00 = 140.00
        cart.addItem(p003(), 3);   // merged: 5 × 70.00 = 350.00

        CHECK(cart.getOrderTotal() == doctest::Approx(350.00));
    }

    TEST_CASE("total updates correctly after removing an item") {
        ShoppingCart cart;
        cart.addItem(p001(), 2);   // 2 × 65.00 = 130.00
        cart.addItem(p005(), 1);   // 1 × 75.00 =  75.00  total = 205.00
        cart.removeItem("P005");   // remove 75.00  → total = 130.00

        CHECK(cart.getOrderTotal() == doctest::Approx(130.00));
    }

    TEST_CASE("total updates correctly after updating a quantity") {
        ShoppingCart cart;
        cart.addItem(p002(), 4);   // 4 × 70.00 = 280.00
        cart.updateItem("P002", 2); // 2 × 70.00 = 140.00

        CHECK(cart.getOrderTotal() == doctest::Approx(140.00));
    }

    TEST_CASE("all five products at qty 1 — total equals sum of all prices") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);   // 65.00
        cart.addItem(p002(), 1);   // 70.00
        cart.addItem(p003(), 1);   // 70.00
        cart.addItem(p004(), 1);   // 75.00
        cart.addItem(p005(), 1);   // 75.00
        // Total: 65 + 70 + 70 + 75 + 75 = 355.00

        CHECK(cart.getOrderTotal() == doctest::Approx(355.00));
    }
}

// ---------------------------------------------------------------------------
// clear()
// ---------------------------------------------------------------------------

TEST_SUITE("ShoppingCart::clear") {

    TEST_CASE("clear empties a non-empty cart") {
        ShoppingCart cart;
        cart.addItem(p001(), 1);
        cart.addItem(p002(), 2);
        cart.clear();

        CHECK(cart.isEmpty() == true);
        CHECK(cart.getItems().size() == 0);
    }

    TEST_CASE("clear on an already-empty cart is a no-op") {
        ShoppingCart cart;
        cart.clear();   // should not throw or crash
        CHECK(cart.isEmpty() == true);
    }

    TEST_CASE("total is zero after clear") {
        ShoppingCart cart;
        cart.addItem(p003(), 5);
        cart.clear();

        CHECK(cart.getOrderTotal() == doctest::Approx(0.00));
    }

    TEST_CASE("cart can be reused after clear") {
        ShoppingCart cart;
        cart.addItem(p001(), 2);
        cart.clear();

        cart.addItem(p005(), 3);   // 3 × PHP 75.00 = PHP 225.00
        REQUIRE(cart.getItems().size() == 1);
        CHECK(cart.getItems()[0].getProductId() == "P005");
        CHECK(cart.getOrderTotal() == doctest::Approx(225.00));
    }
}
