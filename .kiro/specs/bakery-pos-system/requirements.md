# Requirements Document

## Introduction

This document specifies the requirements for the Crombi Bakery Point-of-Sale (POS) system, a console-based C++17 application for a small bakery. The system allows a cashier to browse a hardcoded product catalog, build a shopping cart, manage item quantities, process cash payments, and generate printed receipts. All data is held in memory for the duration of a session. The application must never crash on invalid input and must always allow the user to retry after any error.

## Glossary

- **POS**: The Point-of-Sale application as a whole; the executable program that the cashier operates.
- **Product**: A single bakery item in the catalog, identified by a unique Product ID, with a name and a fixed PHP price.
- **Product_ID**: A string identifier of the form `P001`–`P005` that uniquely identifies a Product in the catalog.
- **Catalog**: The fixed, in-memory collection of five Products available for sale. The Catalog does not change at runtime.
- **Cart**: The in-memory collection of CartItems representing the current transaction. The Cart is cleared after a successful checkout.
- **CartItem**: A pairing of a Product with a positive integer quantity held in the Cart.
- **Subtotal**: The result of multiplying a CartItem's unit price by its quantity.
- **Order_Total**: The sum of all CartItem Subtotals in the Cart.
- **Transaction_ID**: A sequential integer identifier assigned to each completed checkout, starting at 1 and incrementing by 1 per session.
- **Receipt**: The formatted text summary of a completed transaction, printed to the console.
- **Cashier**: The human operator who interacts with the POS via the console.
- **Main_Menu**: The numbered list of seven options presented to the Cashier after each action is completed.
- **Input_Validator**: The component responsible for detecting and rejecting invalid user input.
- **ShoppingCart**: The class that manages the Cart's contents and all Cart operations.
- **Product_Class**: The class that represents a single Product.
- **CartItem_Class**: The class that pairs a Product with a quantity.
- **POS_Class**: The top-level class that owns the Catalog, the ShoppingCart, and the transaction counter, and that drives the main application loop.

---

## Requirements

### Requirement 1: Product Catalog

**User Story:** As a Cashier, I want to view all available bakery products, so that I can inform customers and select items to add to their order.

#### Acceptance Criteria

1. THE POS SHALL initialize the Catalog with exactly five Products on startup: `P001 Chocolate Chip Cookie PHP 65.00`, `P002 Double Chocolate Cookie PHP 70.00`, `P003 Red Velvet Cookie PHP 70.00`, `P004 S'mores Cookie PHP 75.00`, `P005 Brookies PHP 75.00`.
2. WHEN the Cashier selects option 1 from the Main_Menu, THE POS SHALL display all Catalog Products in a table showing Product_ID, name, and unit price formatted as `PHP X.XX`.
3. THE Catalog SHALL remain unchanged for the entire duration of a session.

---

### Requirement 2: Main Menu Navigation

**User Story:** As a Cashier, I want a clear main menu after every action, so that I can choose the next operation without restarting the program.

#### Acceptance Criteria

1. WHEN the POS starts, THE Main_Menu SHALL display the following seven numbered options: `1. View Products`, `2. Add Product to Cart`, `3. View Shopping Cart`, `4. Remove Product from Cart`, `5. Update Product Quantity`, `6. Checkout`, `7. Exit`.
2. WHEN an action completes, THE POS SHALL redisplay the Main_Menu.
3. WHEN the Cashier enters a selection that is not an integer in the range 1–7, THE Input_Validator SHALL display an error message and THE POS SHALL redisplay the Main_Menu without performing any action.
4. WHEN the Cashier selects option 7, THE POS SHALL terminate the program.

---

### Requirement 3: Add Product to Cart

**User Story:** As a Cashier, I want to add a product to the cart by Product ID and quantity, so that I can build the customer's order.

#### Acceptance Criteria

1. WHEN the Cashier selects option 2, THE POS SHALL prompt the Cashier to enter a Product_ID.
2. IF the entered Product_ID does not match any Product in the Catalog, THEN THE Input_Validator SHALL display an error message and THE POS SHALL return to the Main_Menu without modifying the Cart.
3. WHEN a valid Product_ID is entered, THE POS SHALL prompt the Cashier to enter a quantity.
4. IF the entered quantity is not a positive integer, THEN THE Input_Validator SHALL display an error message and THE POS SHALL return to the Main_Menu without modifying the Cart.
5. WHEN the entered Product_ID matches a Product not already in the Cart and the quantity is a positive integer, THE ShoppingCart SHALL add a new CartItem with that Product and quantity.
6. WHEN the entered Product_ID matches a Product already in the Cart and the quantity is a positive integer, THE ShoppingCart SHALL increase the existing CartItem's quantity by the entered amount.
7. WHEN a CartItem is successfully added or updated, THE POS SHALL display a confirmation message.

---

### Requirement 4: Shopping Cart Display

**User Story:** As a Cashier, I want to view the current cart contents, so that I can verify the order before checkout.

#### Acceptance Criteria

1. WHEN the Cashier selects option 3 and the Cart contains at least one CartItem, THE POS SHALL display a table with columns: Product_ID, name, unit price, quantity, and Subtotal for each CartItem, followed by the Order_Total.
2. WHEN the Cashier selects option 3 and the Cart is empty, THE POS SHALL display the message `Cart is empty.`
3. THE POS SHALL format all monetary values in the Cart display as `PHP X.XX` with exactly two decimal places.

---

### Requirement 5: Remove Product from Cart

**User Story:** As a Cashier, I want to remove a product from the cart by Product ID, so that I can correct ordering mistakes.

#### Acceptance Criteria

1. WHEN the Cashier selects option 4, THE POS SHALL prompt the Cashier to enter a Product_ID.
2. IF the entered Product_ID does not match any CartItem in the Cart, THEN THE Input_Validator SHALL display an error message and THE POS SHALL return to the Main_Menu without modifying the Cart.
3. WHEN the entered Product_ID matches a CartItem in the Cart, THE ShoppingCart SHALL remove that CartItem entirely and THE POS SHALL display a confirmation message.

---

### Requirement 6: Update Product Quantity in Cart

**User Story:** As a Cashier, I want to change the quantity of a product already in the cart, so that I can correct quantity mistakes without removing and re-adding the item.

#### Acceptance Criteria

1. WHEN the Cashier selects option 5, THE POS SHALL prompt the Cashier to enter a Product_ID.
2. IF the entered Product_ID does not match any CartItem in the Cart, THEN THE Input_Validator SHALL display an error message and THE POS SHALL return to the Main_Menu without modifying the Cart.
3. WHEN a matching CartItem is found, THE POS SHALL prompt the Cashier to enter a new quantity.
4. IF the entered quantity is not a positive integer, THEN THE Input_Validator SHALL display an error message and THE POS SHALL return to the Main_Menu without modifying the Cart.
5. WHEN a valid Product_ID and positive integer quantity are entered, THE ShoppingCart SHALL replace the CartItem's quantity with the new value and recalculate its Subtotal.

---

### Requirement 7: Checkout and Payment Processing

**User Story:** As a Cashier, I want to process a cash payment for the current cart, so that I can complete the customer's transaction.

#### Acceptance Criteria

1. WHEN the Cashier selects option 6 and the Cart is empty, THE Input_Validator SHALL display an error message and THE POS SHALL return to the Main_Menu without proceeding to payment.
2. WHEN the Cashier selects option 6 and the Cart contains at least one CartItem, THE POS SHALL display a full order summary listing all CartItems with their quantities, unit prices, Subtotals, and the Order_Total.
3. WHEN the order summary is displayed, THE POS SHALL prompt the Cashier to enter the cash amount received.
4. IF the entered cash amount is not a numeric value greater than or equal to the Order_Total, THEN THE Input_Validator SHALL display an error message and THE POS SHALL re-prompt the Cashier for the cash amount.
5. WHEN a valid cash amount is entered, THE POS SHALL calculate the change as the difference between the cash amount and the Order_Total and display it formatted as `PHP X.XX`.

---

### Requirement 8: Receipt Generation

**User Story:** As a Cashier, I want a formatted receipt printed to the console after every successful checkout, so that I can confirm the transaction details.

#### Acceptance Criteria

1. WHEN a checkout is completed successfully, THE POS SHALL print a Receipt to the console.
2. THE Receipt SHALL include the header `Crombi Bakery`, the Transaction_ID, the transaction date and time obtained from the system clock, all CartItems with quantity, unit price, and Subtotal, the Order_Total, the cash amount received, and the change.
3. THE Receipt SHALL include a thank-you message after the change line.
4. WHEN a Receipt is printed, THE POS SHALL increment the Transaction_ID counter by 1 for the next transaction.
5. WHEN a Receipt is printed, THE ShoppingCart SHALL clear all CartItems.

---

### Requirement 9: Input Validation and Error Handling

**User Story:** As a Cashier, I want the system to handle all invalid inputs gracefully, so that the application never crashes and I can always retry.

#### Acceptance Criteria

1. WHEN the Cashier enters a non-numeric value where a number is expected, THE Input_Validator SHALL clear the input stream, display a descriptive error message, and THE POS SHALL allow the Cashier to retry or return to the Main_Menu.
2. WHEN the Cashier enters a numeric value outside the valid range for the current prompt, THE Input_Validator SHALL display a descriptive error message and THE POS SHALL allow the Cashier to retry or return to the Main_Menu.
3. THE POS SHALL remain operational after any invalid input and SHALL NOT terminate abnormally.

---

### Requirement 10: Program Structure and Output Formatting

**User Story:** As a developer, I want the system organized into well-defined classes and files, so that the code is maintainable and understandable.

#### Acceptance Criteria

1. THE POS SHALL be implemented across the following files: `main.cpp`, `Product.h`, `Product.cpp`, `CartItem.h`, `CartItem.cpp`, `ShoppingCart.h`, `ShoppingCart.cpp`, `POS.h`, `POS.cpp`, and `README.md`.
2. THE Product_Class SHALL declare its interface in `Product.h` and define its implementation in `Product.cpp`.
3. THE CartItem_Class SHALL declare its interface in `CartItem.h` and define its implementation in `CartItem.cpp`.
4. THE ShoppingCart SHALL declare its interface in `ShoppingCart.h` and define its implementation in `ShoppingCart.cpp`.
5. THE POS_Class SHALL declare its interface in `POS.h` and define its implementation in `POS.cpp`.
6. THE `main.cpp` file SHALL only instantiate the POS_Class and invoke the method that starts the application loop.
7. THE POS SHALL format all currency values as `PHP X.XX` with exactly two decimal places throughout all menus, tables, and the Receipt.
8. THE POS SHALL use column-aligned table formatting for the product catalog display, cart display, and Receipt item list.

---

# Phase 2 Requirements

## Phase 2 Introduction

Phase 2 preserves the console POS workflow and adds SQLite persistence, product and inventory management, and transaction reporting. Phase 1 behavior remains valid: invalid input must not corrupt state or terminate the application, and checkout must produce a formatted receipt.

## Compatibility and Scope

- The existing `Product`, `CartItem`, and `ShoppingCart` model classes remain in use.
- The fixed startup catalog is replaced by database-backed active products; the five Phase 1 products are seed data for an initial database.
- The existing in-memory cart remains authoritative during menu navigation; checkout is the persistence boundary.
- The Phase 1 transaction counter becomes a database-assigned transaction ID.
- Phase 2 adds console menu choices and management submenus without replacing the existing workflow.

### Requirement 11: SQLite Database Foundation

**User Story:** As a developer, I want the POS backed by SQLite, so that products, inventory, and transactions persist between runs.

#### Acceptance Criteria

1. THE POS SHALL open a local SQLite database and initialize required tables on startup.
2. IF initialization fails, THE POS SHALL display a descriptive error and SHALL NOT crash or silently continue.
3. THE database SHALL create the five Phase 1 products as initial seed data only when no product data exists.
4. THE database layer SHALL use prepared statements for parameterized SQL.
5. THE database connection SHALL be owned and managed by `DatabaseManager`.

### Requirement 12: Product Management

**User Story:** As an administrator, I want to manage the product catalog, so that it remains accurate.

#### Acceptance Criteria

1. THE POS SHALL allow a product to be added with a unique ID, name, price, and initial stock.
2. THE POS SHALL allow an existing active product's name and price to be edited.
3. THE POS SHALL prevent duplicate active or inactive product IDs.
4. THE POS SHALL deactivate a product instead of deleting historical product or transaction data.
5. THE POS SHALL search products by ID or name and display matching active or specified inactive records.

### Requirement 13: Inventory Management

**User Story:** As a cashier or administrator, I want to track stock, so that sales and adjustments remain accurate.

#### Acceptance Criteria

1. THE POS SHALL maintain one current stock quantity per product.
2. THE POS SHALL reject cart additions if the requested quantity exceeds available stock.
3. THE POS SHALL allow restocking and positive or negative stock adjustments with a reason.
4. THE POS SHALL log every stock adjustment with product, quantity change, reason, timestamp, and resulting stock.
5. THE POS SHALL prevent stock from becoming negative.

### Requirement 14: Atomic Checkout

**User Story:** As a cashier, I want checkout to update all data consistently, so that a transaction is never partially saved.

#### Acceptance Criteria

1. WHEN checkout is valid, THE POS SHALL execute transaction saving, item saving, stock deduction, and inventory logging inside one SQLite transaction.
2. IF any checkout step fails, THE POS SHALL roll back every database change, retain the cart, and report the failure.
3. THE POS SHALL recheck stock before committing checkout.
4. THE POS SHALL generate and clear the cart only after the database transaction commits successfully.
5. THE transaction counter SHALL be database-backed and increment for each committed transaction.

### Requirement 15: Historical Transaction Data

**User Story:** As an operator, I want transaction records preserved, so that later edits cannot change history.

#### Acceptance Criteria

1. THE POS SHALL save transaction date/time, cash received, change, and order total.
2. THE POS SHALL save a product ID, product name snapshot, unit price snapshot, quantity, and line subtotal for every transaction item.
3. Editing or deactivating a product SHALL NOT change existing transaction item snapshots.

### Requirement 16: History and Sales Reports

**User Story:** As an operator, I want history and reports, so that I can review sales.

#### Acceptance Criteria

1. THE POS SHALL list transactions by date range or newest-first with transaction ID and totals.
2. THE POS SHALL display a selected transaction with every historical item.
3. THE POS SHALL produce a basic sales summary for a date range: transaction count, gross sales, and item quantities sold.
4. THE POS SHALL exclude deactivated products only where explicitly filtering active catalog data; historical sales SHALL remain visible.
