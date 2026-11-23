/**
 * test_unit_product.cpp
 * ---------------------
 * Unit tests for the Product class.
 *
 * Validates:
 *   - Constructor stores id, name, and price correctly  (Requirements 1.1, 10.2)
 *   - Getters return exactly the values passed at construction (Requirements 1.1, 10.2)
 *
 * Framework: doctest (header-only)
 * Compile:
 *   g++ -std=c++17 -I. -Itests tests/test_unit_product.cpp Product.cpp -o tests/test_unit_product
 */

#include "doctest.h"
#include "Product.h"

// ---------------------------------------------------------------------------
// Constructor / getter round-trip
// ---------------------------------------------------------------------------

TEST_CASE("Product stores id passed at construction") {
    Product p("P001", "Chocolate Chip Cookie", 65.0);
    CHECK(p.getId() == "P001");
}

TEST_CASE("Product stores name passed at construction") {
    Product p("P002", "Double Chocolate Cookie", 70.0);
    CHECK(p.getName() == "Double Chocolate Cookie");
}

TEST_CASE("Product stores price passed at construction") {
    Product p("P003", "Red Velvet Cookie", 70.0);
    CHECK(p.getPrice() == doctest::Approx(70.0));
}

TEST_CASE("Product getters return all values passed at construction") {
    const std::string id    = "P004";
    const std::string name  = "S'mores Cookie";
    const double      price = 75.0;

    Product p(id, name, price);

    CHECK(p.getId()    == id);
    CHECK(p.getName()  == name);
    CHECK(p.getPrice() == doctest::Approx(price));
}

// ---------------------------------------------------------------------------
// Immutability — no mutating calls; only confirm const correctness compiles
// ---------------------------------------------------------------------------

TEST_CASE("Product accessors are callable on a const object") {
    const Product p("P005", "Brookies", 75.0);

    CHECK(p.getId()    == "P005");
    CHECK(p.getName()  == "Brookies");
    CHECK(p.getPrice() == doctest::Approx(75.0));
}

// ---------------------------------------------------------------------------
// Edge cases
// ---------------------------------------------------------------------------

TEST_CASE("Product handles empty string id and name") {
    Product p("", "", 0.0);
    CHECK(p.getId()    == "");
    CHECK(p.getName()  == "");
    CHECK(p.getPrice() == doctest::Approx(0.0));
}

TEST_CASE("Product handles fractional price") {
    Product p("P099", "Mini Tart", 37.50);
    CHECK(p.getPrice() == doctest::Approx(37.50));
}
