# Implementation Plan: Crombi Bakery POS System

## Overview

Implement the Crombi Bakery POS as a console-based C++17 application structured across multiple `.h`/`.cpp` file pairs and driven by `main.cpp`. Classes are built bottom-up — `Product` first, then `CartItem`, then `ShoppingCart`, then `POS` — so each layer depends only on what is already complete. Property-based tests use [rapidcheck](https://github.com/emil-e/rapidcheck) and unit tests use [doctest](https://github.com/doctest/doctest).

## Tasks

- [x] 1. Set up project structure and build configuration
  - Create the project folder layout: source files at the root, a `tests/` subdirectory for test files
  - Write `README.md` documenting how to compile the project (e.g., `g++ -std=c++17 *.cpp -o pos`) and how to run the test suite
  - Create a `Makefile` (or equivalent build script) with targets: `all`, `run`, `test`, `clean`
  - Add stub/empty `.h` and `.cpp` files for all required classes so the build system can be verified before any logic is written: `Product.h`, `Product.cpp`, `CartItem.h`, `CartItem.cpp`, `ShoppingCart.h`, `ShoppingCart.cpp`, `POS.h`, `POS.cpp`, `main.cpp`
  - _Requirements: 10.1_

- [x] 2. Implement the `Product` class
  - [x] 2.1 Write `Product.h` and `Product.cpp`
    - Declare and define the constructor `Product(std::string id, std::string name, double price)`
    - Implement `getId()`, `getName()`, `getPrice()` const accessors; no setters
    - _Requirements: 1.1, 10.2_

  - [x] 2.2 Write property test for catalog immutability (Property 1)
    - Confirm that after any sequence of cart operations, the five catalog products remain identical to their initial state
    - **Property 1: Catalog immutability**
    - **Validates: Requirements 1.3**

  - [x] 2.3 Write unit tests for `Product`
    - Verify constructor stores id, name, and price correctly
    - Verify getters return the values passed at construction
    - _Requirements: 1.1, 10.2_

- [x] 3. Implement the `CartItem` class
  - [x] 3.1 Write `CartItem.h` and `CartItem.cpp`
    - Declare and define `CartItem(const Product& product, int quantity)`
    - Implement `getProductId()`, `getProductName()`, `getUnitPrice()`, `getQuantity()`, `getSubtotal()` const accessors
    - Implement `setQuantity(int newQty)` for direct replacement and `addQuantity(int additionalQty)` for merging
    - `getSubtotal()` must compute `unit_price * quantity` on the fly
    - _Requirements: 10.3_

  - [x] 3.2 Write property test for new item addition correctness (Property 5)
    - For any valid product and any positive quantity, verify that the constructed `CartItem` has the correct quantity and that `getSubtotal() == getUnitPrice() * getQuantity()`
    - **Property 5: New item addition correctness**
    - **Validates: Requirements 3.5**

  - [x] 3.3 Write property test for quantity accumulation (Property 6)
    - For any `CartItem` with quantity Q and any positive delta, verify that after `addQuantity(delta)` the quantity equals `Q + delta`
    - **Property 6: Duplicate item quantity accumulation**
    - **Validates: Requirements 3.6**

  - [x] 3.4 Write property test for quantity update correctness (Property 10)
    - For any `CartItem` and any new positive integer N, verify that after `setQuantity(N)` the quantity equals N and `getSubtotal() == getUnitPrice() * N`
    - **Property 10: Quantity update correctness**
    - **Validates: Requirements 6.5_

  - [x] 3.5 Write unit tests for `CartItem`
    - Test `getSubtotal()` with known price/quantity values
    - Test `addQuantity()` accumulation
    - Test `setQuantity()` replacement
    - _Requirements: 10.3_

- [x] 4. Checkpoint — Ensure `Product` and `CartItem` tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 5. Implement the `ShoppingCart` class
  - [x] 5.1 Write `ShoppingCart.h` and `ShoppingCart.cpp`
    - Implement `addItem(const Product& product, int quantity)`: use `findItem` to detect duplicates; call `addQuantity` on existing items or push a new `CartItem`
    - Implement `removeItem(const std::string& productId)`: return `false` if not found
    - Implement `updateItem(const std::string& productId, int newQty)`: return `false` if not found
    - Implement `clear()`, `isEmpty()`, `containsProduct()`, `getOrderTotal()`, `getItems()` const reference
    - Implement the private `findItem()` iterator helper
    - _Requirements: 3.5, 3.6, 5.3, 6.5, 10.4_

  - [x] 5.2 Write property test for invalid Product_ID rejection (Property 3)
    - For any string not in {P001–P005}, calling `addItem`, `removeItem`, or `updateItem` must leave the cart unchanged
    - **Property 3: Invalid Product_ID is always rejected**
    - **Validates: Requirements 3.2, 5.2, 6.2**

  - [x] 5.3 Write property test for removal correctness (Property 9)
    - For any cart item present by Product_ID, after `removeItem` that product must no longer appear and all other items must be unchanged
    - **Property 9: Removal correctness**
    - **Validates: Requirements 5.3**

  - [x] 5.4 Write property test for cart display completeness (Property 8)
    - For any non-empty cart, `getItems()` must contain all added products with correct quantities and `getOrderTotal()` must equal the sum of all subtotals
    - **Property 8: Cart display completeness**
    - **Validates: Requirements 4.1**

  - [x] 5.5 Write unit tests for `ShoppingCart`
    - Test `isEmpty()` on a fresh cart
    - Test adding a new item and then the same item again (merge)
    - Test `removeItem()` on a present and an absent ID
    - Test `updateItem()` on a present and an absent ID
    - Test `getOrderTotal()` with known values
    - Test `clear()` empties the cart
    - _Requirements: 3.5, 3.6, 4.2, 5.3, 6.5_

- [x] 6. Checkpoint — Ensure `ShoppingCart` tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 7. Implement `POS` input validation helpers
  - [x] 7.1 Write the static input helpers in `POS.h`/`POS.cpp`
    - Implement `readMenuChoice()`: parse an integer, return -1 for anything outside [1, 7] or on stream failure; clear and ignore stream on failure
    - Implement `readPositiveInt(const std::string& prompt)`: retry loop until a positive integer is entered; recover stream on failure
    - Implement `readCashAmount(double minimum)`: retry loop until a double ≥ minimum is entered; recover stream on failure
    - Implement `readProductId(const std::string& prompt)`: read a string token from cin
    - _Requirements: 2.3, 3.4, 6.4, 7.4, 9.1, 9.2, 9.3_

  - [x] 7.2 Write property test for invalid menu input rejection (Property 2)
    - For any integer outside [1, 7], `readMenuChoice` must return -1
    - **Property 2: Invalid menu input is always rejected**
    - **Validates: Requirements 2.3**

  - [x] 7.3 Write property test for non-positive quantity rejection (Property 4)
    - Verify that the quantity validation path rejects zero and negative values, leaving the cart unchanged
    - **Property 4: Non-positive quantity is always rejected**
    - **Validates: Requirements 3.4, 6.4**

  - [x] 7.4 Write property test for system resilience to invalid input (Property 16)
    - For any sequence of invalid inputs, the POS must remain operational and the cart state must not be corrupted
    - **Property 16: System survives any invalid input sequence**
    - **Validates: Requirements 9.1, 9.2, 9.3**

- [x] 8. Implement `POS` output helpers and catalog display
  - [x] 8.1 Implement `formatCurrency` and `getCurrentDateTime` helpers
    - `formatCurrency(double amount)`: return `"PHP X.XX"` using `std::ostringstream`, `std::fixed`, `std::setprecision(2)`
    - `getCurrentDateTime()`: return a human-readable timestamp string from `std::ctime` / `std::localtime`
    - _Requirements: 4.3, 10.7_

  - [x] 8.2 Write property test for currency formatting invariant (Property 7)
    - For any non-negative double, the formatted string must match `PHP \d+\.\d{2}`
    - **Property 7: Currency formatting invariant**
    - **Validates: Requirements 4.3, 10.7**

  - [x] 8.3 Implement `POS` constructor and `viewProducts()`
    - Constructor: initialize `transactionId_` to 1 and populate `catalog_` with the five hardcoded `Product` objects
    - `viewProducts()`: print an aligned table (Product_ID | Name | Price) for all catalog entries
    - `findProductById()`: linear search returning `const Product*` or `nullptr`
    - _Requirements: 1.1, 1.2, 10.1, 10.8_

  - [x] 8.4 Implement `viewCart()` in `POS`
    - If cart is empty, print `"Cart is empty."`; otherwise print an aligned table (Product_ID | Name | Unit Price | Qty | Subtotal) plus the Order_Total line
    - _Requirements: 4.1, 4.2, 4.3, 10.8_

- [x] 9. Implement `POS` cart-mutation action handlers
  - [x] 9.1 Implement `addToCart()` in `POS`
    - Prompt for Product_ID using `readProductId`; look up with `findProductById`; display error and return on no match
    - Prompt for quantity using `readPositiveInt`; call `cart_.addItem`; print confirmation
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

  - [x] 9.2 Implement `removeFromCart()` in `POS`
    - Prompt for Product_ID; check `cart_.containsProduct`; display error and return on no match
    - Call `cart_.removeItem`; print confirmation
    - _Requirements: 5.1, 5.2, 5.3_

  - [x] 9.3 Implement `updateQuantity()` in `POS`
    - Prompt for Product_ID; check `cart_.containsProduct`; display error and return on no match
    - Prompt for new quantity using `readPositiveInt`; call `cart_.updateItem`; print confirmation
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5_

- [x] 10. Implement `checkout()`, `printReceipt()`, and the main loop in `POS`
  - [x] 10.1 Implement `checkout()` in `POS`
    - If cart is empty, display error and return
    - Display order summary (all items, quantities, unit prices, subtotals, Order_Total)
    - Use `readCashAmount(orderTotal)` to collect valid cash; compute and display change with `formatCurrency`
    - Call `printReceipt(cashAmount)` then `cart_.clear()` then increment `transactionId_`
    - _Requirements: 7.1, 7.2, 7.3, 7.4, 7.5, 8.4, 8.5_

  - [x] 10.2 Write property test for insufficient cash rejection (Property 11)
    - For any non-empty cart with Order_Total T and cash C < T, verify checkout rejects payment
    - **Property 11: Insufficient cash is always rejected**
    - **Validates: Requirements 7.4**

  - [x] 10.3 Write property test for change calculation correctness (Property 12)
    - For cash C ≥ Order_Total T, verify change == C − T
    - **Property 12: Change calculation correctness**
    - **Validates: Requirements 7.5**

  - [x] 10.4 Implement `printReceipt(double cashAmount)` in `POS`
    - Print store header `Crombi Bakery`, Transaction_ID, timestamp from `getCurrentDateTime()`
    - Print aligned item list (qty × unit price = subtotal), Order_Total, cash amount, change, thank-you message
    - _Requirements: 8.1, 8.2, 8.3, 10.7, 10.8_

  - [x] 10.5 Write property test for receipt completeness (Property 13)
    - For any completed checkout, the receipt string must contain the store header, Transaction_ID, timestamp, all CartItems, Order_Total, cash amount, change, and thank-you message
    - **Property 13: Receipt completeness**
    - **Validates: Requirements 8.1, 8.2, 8.3**

  - [x] 10.6 Write property test for Transaction_ID increment (Property 14)
    - For N successful checkouts, the Transaction_ID after the Nth must equal the starting ID plus N
    - **Property 14: Transaction ID increment**
    - **Validates: Requirements 8.4**

  - [x] 10.7 Write property test for cart cleared after checkout (Property 15)
    - For any non-empty cart that completes checkout, the cart must be empty immediately afterward
    - **Property 15: Cart cleared after checkout**
    - **Validates: Requirements 8.5**

  - [x] 10.8 Implement `POS::run()` — the main menu loop
    - Loop: display Main_Menu, call `readMenuChoice()`, dispatch to the matching handler or display error; break on choice 7
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

- [x] 11. Write `main.cpp`
  - Include only `POS.h`
  - Instantiate `POS pos` and call `pos.run()`
  - Return 0
  - _Requirements: 10.1, 10.6_

- [x] 12. Checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

- [x] 13. Integration and compilation verification
  - [x] 13.1 Verify full compilation
    - Compile the entire project with `g++ -std=c++17 *.cpp -o pos` (or via `Makefile`) and confirm no errors or warnings
    - _Requirements: 10.1_

  - [x] 13.2 Write integration smoke tests
    - Test that five products are initialized with correct IDs, names, and prices
    - Test that an empty-cart checkout shows an error and returns to the menu
    - Test a complete happy-path session: view catalog → add two items → view cart → checkout with sufficient cash → verify receipt fields and empty cart
    - _Requirements: 1.1, 2.1, 4.1, 7.1, 8.1, 8.5_

- [x] 14. Final checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for a faster MVP
- Each task references specific requirements for traceability
- Checkpoints ensure incremental validation at logical boundaries
- Property tests use **rapidcheck**; unit tests use **doctest**
- Each property test must include a comment: `// Feature: bakery-pos-system, Property N: <property text>`
- Each property test must run a minimum of 100 iterations
- Build from the bottom up: `Product` → `CartItem` → `ShoppingCart` → `POS` helpers → `POS` actions → `main.cpp`

## Task Dependency Graph

```json
{
  "waves": [
    { "id": 0, "tasks": ["1"] },
    { "id": 1, "tasks": ["2.1"] },
    { "id": 2, "tasks": ["2.2", "2.3"] },
    { "id": 3, "tasks": ["3.1"] },
    { "id": 4, "tasks": ["3.2", "3.3", "3.4", "3.5"] },
    { "id": 5, "tasks": ["4"] },
    { "id": 6, "tasks": ["5.1"] },
    { "id": 7, "tasks": ["5.2", "5.3", "5.4", "5.5"] },
    { "id": 8, "tasks": ["6"] },
    { "id": 9, "tasks": ["7.1"] },
    { "id": 10, "tasks": ["7.2", "7.3", "7.4", "8.1"] },
    { "id": 11, "tasks": ["8.2", "8.3"] },
    { "id": 12, "tasks": ["8.4"] },
    { "id": 13, "tasks": ["9.1", "9.2", "9.3"] },
    { "id": 14, "tasks": ["10.1"] },
    { "id": 15, "tasks": ["10.2", "10.3", "10.4"] },
    { "id": 16, "tasks": ["10.5", "10.6", "10.7", "10.8"] },
    { "id": 17, "tasks": ["11"] },
    { "id": 18, "tasks": ["12"] },
    { "id": 19, "tasks": ["13.1"] },
    { "id": 20, "tasks": ["13.2"] },
    { "id": 21, "tasks": ["14"] }
  ]
}

---

# Phase 2 Implementation Tasks

## 15. SQLite Foundation

- [x] 15.1 Add `DatabaseManager.h/.cpp` and local SQLite build linkage.
- [x] 15.2 Implement database opening, foreign-key enforcement, schema initialization, prepared statements, and begin/commit/rollback helpers.
- [x] 15.3 Seed the initial five products, then add missing bakery catalog entries without overwriting existing product data or stock.
- [x] 15.4 Add database unit tests using a temporary database.
- Acceptance: clean database initializes reproducibly; rollback and schema failures are handled.

## 16. Product Persistence

- [x] 16.1 Add `ProductRepository.h/.cpp`.
- [x] 16.2 Implement find, search, active listing, create, edit, and deactivate operations.
- [x] 16.3 Provide repository-backed active catalog loading for later POS integration while preserving the existing `Product` model.
- [x] 16.4 Add product repository and duplicate-ID tests.
- Acceptance: product changes persist; deactivation preserves historical rows.

## 17. Inventory Persistence

- [x] 17.1 Add `InventoryRepository.h/.cpp`.
- [x] 17.2 Implement stock reads, restock, signed adjustments, and inventory log creation.
- [x] 17.3 Prevent negative stock and validate adjustment reasons.
- [x] 17.4 Add stock and inventory-log tests.
- Acceptance: all stock mutations persist and are logged atomically.

## 18. Product and Inventory Console Features

- [x] 18.1 Add management submenus without changing menu choices 1–7.
- [x] 18.2 Integrate active-product search/add/edit/deactivate flows.
- [x] 18.3 Integrate restock and stock-adjustment flows.
- [x] 18.4 Reject cart additions and quantity updates that exceed available stock.
- Acceptance: invalid management input is retryable and cart stock checks are consistent.

## 19. Atomic Transactions

- [x] 19.1 Add `TransactionRepository.h/.cpp`.
- [x] 19.2 Implement prepared statement checkout transaction with stock recheck.
- [x] 19.3 Save transaction and transaction-item snapshots.
- [x] 19.4 Deduct stock and log deductions inside the same transaction.
- [x] 19.5 Add rollback tests for item, stock, and inventory-log failures.
- Acceptance: either the complete checkout commits or every change rolls back.

## 20. Checkout Integration

- [x] 20.1 Route valid checkout through `TransactionRepository::createAtomic`.
- [x] 20.2 Clear the cart and increment from the returned database transaction ID only after commit.
- [x] 20.3 Generate receipts from committed data or committed checkout snapshots.
- [x] 20.4 Add successful, insufficient-stock, and forced-failure integration tests.
- Acceptance: receipt and database state agree; failed checkout retains the cart.

## 21. Transaction History and Reports

- [x] 21.1 Add transaction history repository queries and console listing/detail views.
- [x] 21.2 Add date-range sales summary.
- [x] 21.3 Verify historical names and prices remain unchanged after product edits.
- [x] 21.4 Add history and report tests.
- Acceptance: history and reports are complete and historical snapshots are preserved.

## 22. Phase 2 Final Verification

- [x] 22.1 Add an isolated SQLite test database option.
- [x] 22.2 Update `Makefile` and README build/run instructions.
- [x] 22.3 Run clean build and complete unit, property, integration, and database tests.
- [x] 22.4 Update Phase 2 requirements/design/task checkpoints and review remaining risks.
- Acceptance: all existing Phase 1 behavior passes plus the Phase 2 test suite.
```
