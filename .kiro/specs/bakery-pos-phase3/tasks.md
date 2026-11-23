# Phase 3 Implementation Tasks

## 23. Qt and CMake Setup

- [x] Configure Qt 6.12.0 Widgets and SQLite3 with CMake and C++17.
- [x] Keep separate console and GUI application entry points.
- [x] Add a minimal, themed Qt MainWindow while keeping the console target buildable.
- Acceptance: both application targets build and existing tests remain runnable.

## 24. GUI Controller, Main Window, and Navigation

- [x] Reuse POS operations through the Qt-independent `PosController`.
- [x] Add a sidebar and persistent stacked pages for Dashboard, Point of Sale, Products, Inventory, Transactions, and Reports.
- [x] Apply the approved pastel bakery theme and verify page navigation.
- Acceptance: page changes do not recreate the main window, and GUI widgets do not contain SQL or business logic.

## 25. Point of Sale GUI

- [x] Display available products, prices, and stock; support cart quantity changes.
- [x] Show cart subtotals and total and validate cash payment.
- [x] Integrate atomic checkout and show transaction confirmation and receipt data.
- [x] Test successful checkout and failure handling.
- Acceptance: the existing checkout workflow is reused and failed checkout preserves the cart.

## 26. Products and Inventory GUI

- [x] Display product details and support search, add, edit, and deactivate actions.
- [x] Display stock levels with low-stock and out-of-stock indicators.
- [x] Support restocking and signed stock adjustments with required reasons.
- [x] Use existing controller and repository validation and inventory logging.
- [x] Verify product and inventory changes persist after reopening the database.
- Acceptance: GUI changes use the existing repositories, inactive products cannot be changed in inventory, and stock cannot become negative.

## 27. Transaction History and Sales Reports

- [x] Display recent transactions and search by ID; show details and receipts from stored snapshots.
- [x] Display daily sales, completed transaction counts, products sold, and product summaries.
- [x] Add an isolated Qt smoke test and verify displayed history/report values against the existing database.
- Acceptance: only committed transaction data is displayed through existing repository operations.

## 28. Final Verification and Project Review

- [x] Perform a clean rebuild and run the complete unit, integration, and property test suites.
- [x] Verify persistence, atomic checkout, rollback, and database integrity.
- [x] Review validation/error handling and update setup and usage documentation.
- [x] Record the Phase 3 completion checkpoint.
- Acceptance: all Phase 3 features pass their specified checks without regressing console behavior.

## UI Polish and Branding Checkpoint

- [x] Replaced the empty dashboard with database-backed KPIs, recent transactions, and low-stock products.
- [x] Audited the shared widget theme and improved input, table, button, focus, hover, pressed, and disabled states.
- [x] Integrated the supplied CROMBI COOKIES logo through Qt resources with a text fallback.
- [x] Updated build and launch documentation and verified the complete GUI navigation and workflow suites.
