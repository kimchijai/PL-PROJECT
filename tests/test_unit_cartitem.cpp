/**
 * test_unit_cartitem.cpp
 * ----------------------
 * Unit tests for the CartItem class.
 *
 * Validates: Requirements 10.3
 *
 * Coverage:
 *   - getSubtotal()   : computed correctly from known price / quantity pairs
 *   - addQuantity()   : accumulates onto the existing quantity
 *   - setQuantity()   : replaces the existing quantity
 */

#include "doctest.h"

#include "Product.h"
#include "CartItem.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// A reusable product at PHP 65.00 (Chocolate Chip Cookie, as in the design doc)
static Product makeCookie() {
    return Product("P001", "Chocolate Chip Cookie", 65.00);
}

// A product whose price has fractional cents to exercise floating-point paths
static Product makeFrenchBaguette() {
    return Product("P002", "French Baguette", 125.50);
}

// ---------------------------------------------------------------------------
// getSubtotal()
// ---------------------------------------------------------------------------

TEST_SUITE("CartItem::getSubtotal") {

    TEST_CASE("single unit — subtotal equals unit price") {
        CartItem item(makeCookie(), 1);
        CHECK(item.getSubtotal() == doctest::Approx(65.00));
    }

    TEST_CASE("multiple units — subtotal is price × quantity") {
        CartItem item(makeCookie(), 3);
        // 3 × PHP 65.00 = PHP 195.00
        CHECK(item.getSubtotal() == doctest::Approx(195.00));
    }

    TEST_CASE("fractional price — subtotal rounds correctly") {
        CartItem item(makeFrenchBaguette(), 4);
        // 4 × PHP 125.50 = PHP 502.00
        CHECK(item.getSubtotal() == doctest::Approx(502.00));
    }

    TEST_CASE("large quantity — subtotal scales linearly") {
        CartItem item(makeCookie(), 100);
        // 100 × PHP 65.00 = PHP 6 500.00
        CHECK(item.getSubtotal() == doctest::Approx(6500.00));
    }
}

// ---------------------------------------------------------------------------
// addQuantity()
// ---------------------------------------------------------------------------

TEST_SUITE("CartItem::addQuantity") {

    TEST_CASE("adds to existing quantity") {
        CartItem item(makeCookie(), 2);
        item.addQuantity(3);
        CHECK(item.getQuantity() == 5);
    }

    TEST_CASE("subtotal updates after addQuantity") {
        CartItem item(makeCookie(), 2);   // 2 × 65.00 = 130.00
        item.addQuantity(3);              // now 5 × 65.00 = 325.00
        CHECK(item.getSubtotal() == doctest::Approx(325.00));
    }

    TEST_CASE("successive addQuantity calls accumulate") {
        CartItem item(makeCookie(), 1);
        item.addQuantity(2);  // 3
        item.addQuantity(4);  // 7
        CHECK(item.getQuantity() == 7);
        CHECK(item.getSubtotal() == doctest::Approx(455.00)); // 7 × 65.00
    }

    TEST_CASE("adding 1 increments by exactly one") {
        CartItem item(makeCookie(), 10);
        item.addQuantity(1);
        CHECK(item.getQuantity() == 11);
    }
}

// ---------------------------------------------------------------------------
// setQuantity()
// ---------------------------------------------------------------------------

TEST_SUITE("CartItem::setQuantity") {

    TEST_CASE("replaces quantity with new value") {
        CartItem item(makeCookie(), 5);
        item.setQuantity(2);
        CHECK(item.getQuantity() == 2);
    }

    TEST_CASE("subtotal updates after setQuantity") {
        CartItem item(makeCookie(), 5);  // 5 × 65.00 = 325.00
        item.setQuantity(2);             // 2 × 65.00 = 130.00
        CHECK(item.getSubtotal() == doctest::Approx(130.00));
    }

    TEST_CASE("setQuantity does not accumulate — it replaces") {
        CartItem item(makeCookie(), 3);
        item.setQuantity(7);
        item.setQuantity(4);  // should be 4, not 7+4 or 3+7+4
        CHECK(item.getQuantity() == 4);
    }

    TEST_CASE("setQuantity to 1 gives subtotal equal to unit price") {
        CartItem item(makeFrenchBaguette(), 10);
        item.setQuantity(1);
        CHECK(item.getSubtotal() == doctest::Approx(item.getUnitPrice()));
    }
}

// ---------------------------------------------------------------------------
// Accessor correctness (sanity checks for the product data stored in item)
// ---------------------------------------------------------------------------

TEST_SUITE("CartItem accessors") {

    TEST_CASE("stores product id, name, and price correctly") {
        Product p("P003", "Strawberry Tart", 95.00);
        CartItem item(p, 2);

        CHECK(item.getProductId()   == "P003");
        CHECK(item.getProductName() == "Strawberry Tart");
        CHECK(item.getUnitPrice()   == doctest::Approx(95.00));
        CHECK(item.getQuantity()    == 2);
    }
}
