#include "doctest.h"
#include "../POS.h"

#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>

TEST_CASE("Integration: complete POS workflow") {
    const char* tempDb = "tests/phase2_checkout_integration.db";
    std::remove(tempDb);
    std::istringstream input(
        "1\n"
        "2\nP001\n2\n"
        "5\nP001\n1\n"
        "3\n"
        "4\nP001\n"
        "2\nP001\n1\n"
        "2\nP002\n2\n"
        "3\n"
        "6\nabc\n10\n205\n"
        "2\nP003\n1\n"
        "6\n70\n"
        "3\n"
        "7\n"
    );

    std::ostringstream output;
    std::streambuf* oldInput = std::cin.rdbuf(input.rdbuf());
    std::streambuf* oldOutput = std::cout.rdbuf(output.rdbuf());

    {
        POS pos(tempDb);
        std::string operatorError;
        REQUIRE(pos.createInitialAdministrator(
            "integration_admin", "IntegrationTestPassword!23", operatorError));
        REQUIRE(pos.login("integration_admin",
                          "IntegrationTestPassword!23", operatorError));
        pos.run();
    }

    std::cout.rdbuf(oldOutput);
    std::cin.rdbuf(oldInput);

    const std::string result = output.str();

    CHECK(result.find("Chocolate Chip Cookie") != std::string::npos);
    CHECK(result.find("Double Chocolate Cookie") != std::string::npos);
    CHECK(result.find("Red Velvet Cookie") != std::string::npos);
    CHECK(result.find("Updated P001 quantity to 1.") != std::string::npos);
    CHECK(result.find("Removed P001 from cart.") != std::string::npos);
    CHECK(result.find("Added Chocolate Chip Cookie (x1) to cart.") != std::string::npos);
    CHECK(result.find("Added Double Chocolate Cookie (x2) to cart.") != std::string::npos);
    CHECK(result.find("Invalid input. Please enter a valid amount.") != std::string::npos);
    CHECK(result.find("Amount must be at least PHP 205.00.") != std::string::npos);
    CHECK(result.find("Change: PHP 0.00") != std::string::npos);
    CHECK(result.find("Transaction ID: 1") != std::string::npos);
    CHECK(result.find("Transaction ID: 2") != std::string::npos);
    CHECK(result.find("Cart is empty.") != std::string::npos);
    CHECK(result.find("Thank you for your purchase!") != std::string::npos);
    CHECK(result.find("Goodbye!") != std::string::npos);
    std::remove(tempDb);
}
