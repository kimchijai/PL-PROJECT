# Phase 3 Requirements

## Scope

Replace the executable's console front end with a Qt Widgets desktop GUI while preserving the Phase 1 cart workflow and Phase 2 SQLite repositories. Business rules, repositories, models, and database behavior remain separate from Qt classes.

## Compatibility

- Preserve `Product`, `CartItem`, `ShoppingCart`, `DatabaseManager`, `ProductRepository`, `InventoryRepository`, and `TransactionRepository`.
- Preserve the default `bakery_pos.db` location and existing schema.
- Preserve console `POS::run()` for Phase 1/2 compatibility and testing unless it is superseded by a deliberate, reviewed entry-point split.
- Keep C++17, SQLite prepared statements, foreign keys, and atomic checkout unchanged.
- Do not implement authentication, network features, GUI frameworks other than Qt Widgets, or Qt Quick.

## Requirements

### R1: Qt Application Shell

- A Qt Widgets application shall start with a professional main window.
- The shell shall use responsive layouts and expose POS, Products, Inventory, and History/Reports areas.
- Closing the application shall close the database cleanly.

### R2: POS and Checkout

- The GUI shall list active products, add and merge cart items, update quantities, remove items, and show totals.
- The GUI shall prevent inactive products, insufficient stock, and invalid quantities.
- Checkout shall collect cash, validate payment, display change, use the atomic repository workflow, and show a receipt only after a successful commit.
- A failed checkout shall preserve the cart and display a clear error.

### R3: Product and Inventory Management

- The GUI shall support active product viewing/search, create, edit, deactivate, restock, and signed stock adjustments.
- Inventory views shall identify out-of-stock and low-stock items.
- Every stock movement shall continue to use `InventoryRepository`.

### R4: History and Reports

- The GUI shall list transactions, search by ID, show transaction details, regenerate receipts from snapshots, and show daily sales summaries.
- Historical names and prices shall come from transaction snapshots.

### R5: Separation and Reliability

- Qt widgets shall not execute SQL or contain repository implementation logic.
- The GUI controller/service shall translate repository results into UI models and errors.
- Invalid input shall remain retryable and shall not crash or close the application.
- Qt object lifetime shall not outlive repository/database members.

### R6: Testing and Build

- Existing doctest unit/property/integration tests shall continue to run.
- Qt build configuration shall support CMake, Qt 6 Core/Widgets/Gui, optional Qt Test, and SQLite3.
- GUI logic shall have unit tests separate from visual rendering where practical.

### R7: Bakery Catalog Source

- The POS page shall load active product names, prices, and stock from the existing SQLite database through the controller; it shall not substitute generic or duplicated sample products.
- First-run catalog seeding shall preserve `P001`–`P005` and add `P006 Nutella` at PHP 65.00, `P007 Dubai Chewy Cookie (1 Piece)` at PHP 110.00, and `P008 Dubai Chewy Cookie (4 Pieces)` at PHP 420.00, all in the existing Cookies category.
- Each newly seeded product, including each Dubai Chewy Cookie size option, shall have 100 initial stock units and an independent inventory record.
- Startup seeding shall add only missing products and initial inventory. Existing product records, categories, prices, and stock shall remain unchanged on later starts.
