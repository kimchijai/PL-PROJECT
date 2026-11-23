# Crombi Bakery POS System

A C++17 bakery Point-of-Sale application with a console interface and a Qt Widgets desktop interface. It uses an in-memory shopping cart and SQLite for products, inventory, inventory logs, transactions, and transaction snapshots.

---

## Project Structure

```
PL PROJECT/
├── main.cpp            # Entry point — instantiates POS and starts the loop
├── Product.h / .cpp    # Product data class (id, name, price)
├── CartItem.h / .cpp   # Pairs a Product with a quantity
├── ShoppingCart.h/.cpp # Manages the collection of CartItems
├── POS.h / .cpp        # Top-level app class: catalog, cart, menu loop
├── DatabaseManager.h/.cpp
├── OperatorAccountRepository.h/.cpp # Local operator accounts and password hashes
├── ProductRepository.h/.cpp
├── InventoryRepository.h/.cpp
├── TransactionRepository.h/.cpp
├── CMakeLists.txt       # Qt 6.12 GUI and console build targets
├── gui/                 # Qt Widgets pages and business controller
├── Makefile            # Build targets: all, run, test, clean
├── README.md           # This file
└── tests/              # Unit and property-based test files
    └── *.cpp           # Unit, property-style, and integration tests
```

---

## Prerequisites

- **g++** with C++17 support (MinGW on Windows, or any GCC ≥ 7)
- **GNU Make** (comes with MinGW; install via `winget install GnuWin32.Make` or similar)
- **doctest** header — download `doctest.h` and place it in `tests/`
- **SQLite3** development headers and library (`sqlite3.h`, `libsqlite3`)
- **OpenSSL libcrypto** on non-Windows platforms; Windows uses the built-in CNG
  BCrypt provider
- **Qt 6.12.0** Widgets and a matching MinGW toolchain
- **CMake 3.21+** and Ninja (or another supported CMake generator)

The SQLite library must be visible to the compiler and runtime linker. On MinGW/MSYS2, install development packages such as `mingw-w64-ucrt-x86_64-sqlite3` if they are not already available.

Verify your toolchain:

```powershell
g++ --version
make --version
```

The application uses `-lsqlite3` and requires the SQLite3 development library.
The console application uses `bakery_pos.db` in its working directory. The GUI
stores its database and uploaded product images in the per-user application data
directory (`%LOCALAPPDATA%\Crombi Cookies\Crombi Bakery POS` on Windows). On
first GUI launch, an existing working-directory or development-workspace
database and its uploaded images are copied there; the originals are not
removed. The first run creates the schema and seeds the bakery catalog. The
desktop app requires creating an administrator account on first launch, then
requires a username and password at each launch. Passwords are stored as salted
PBKDF2-HMAC-SHA-256 hashes, never as plaintext. Administrators can create, edit,
and deactivate operator accounts from **Manage operators**; deactivation
preserves historical transaction and inventory attribution. On an existing
database, initialization adds only missing seeded products and their initial
inventory; existing product records and stock levels are preserved.
Tests use temporary database files and remove them after execution.

---

## Building

### Qt 6 Desktop and Console Applications

The `qt-6.12.0-mingw` CMake preset pins the installed Qt package, its
`Widgets` and `PrintSupport` components, the existing Qt MinGW compiler,
Ninja, and SQLite paths. VS Code selects this preset automatically:

```powershell
cmake --preset qt-6.12.0-mingw-release
cmake --build --preset qt-6.12.0-mingw-release
ctest --preset qt-6.12.0-mingw-release
```

The Release GUI executable is generated at
`build\release\bakery_pos_gui.exe`. The console target continues to use the
current working directory for its database; the GUI uses per-user application
data regardless of its working directory:

```powershell
.\build\release\bakery_pos_console.exe
.\build\release\bakery_pos_gui.exe
```

The unbundled Release GUI needs Qt, SQLite, and matching MinGW runtime DLLs
available on `PATH`. It is not a distributable package; deployment is a
separate step using `windeployqt.exe` plus the SQLite and MinGW runtime files.
For a local build, add the matching runtime directories before launch:

```powershell
$env:PATH = "C:\Qt\6.12.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\msys64\ucrt64\bin;" + $env:PATH
.\build\release\bakery_pos_gui.exe
```

### Portable Windows package

After building Release, run `.\packaging\create-portable-package.ps1`. It creates
`dist\CrombiCookiesPOS-Portable.zip` with Qt deployment files, SQLite, licensed
UI fonts, product assets, and a snapshot of the workspace database. The package
uses `data\bakery_pos.db` and `data\assets\products\uploads`; keep `data` with
the application folder. Portable startup refuses to run if its database is
missing or empty and never replaces an existing package database.

The GUI includes working Point of Sale, Products, and Inventory pages. Product
management supports adding, editing, and deactivating products; inventory
supports restocking and signed stock adjustments with recorded reasons. Stock
levels of 30 or fewer are marked low, and zero stock is marked out of stock.
Transactions displays recent history, searches by transaction ID, and rebuilds
receipts from saved item snapshots. Reports displays daily transaction/product
counts, gross sales, and product sales summaries using the existing reporting
repository.
The header provides database backup and restore controls. Backups use SQLite's
backup API, backup files are validated before restore, and a pre-restoration
safety backup is created automatically. Reports can be exported as CSV/PDF or
sent to an available printer. Historical receipts can be exported as PDF or
printed using saved transaction snapshots.
The Dashboard shows today's sales and transaction count, active catalog size,
low-stock alerts, recent transactions, and low-stock product records from the
same SQLite database.

The supplied CROMBI COOKIES logo is loaded through `assets/assets.qrc` as
`:/assets/crombi-logo.png`. It scales proportionally in the sidebar without
changing the attached image. If the
asset is removed or cannot be loaded, the clean "CROMBI COOKIES" text fallback
remains visible.

The POS catalog uses supplied photos from `assets/products/`. Image mappings
use product IDs and embedded resources for the seeded catalog; uploaded images
are stored under the per-user application data directory. Missing images use a
clean neutral placeholder rather than substituted or generated photos.

### Using Make

```bash
# Clean, build, and run the application
make clean
make all
make run

# Build and run the complete unit, property, and integration test suite
make test
```

### Manual compilation (no Make)

```bash
# Main application (Windows)
g++ -std=c++17 -Wall -I. Product.cpp CartItem.cpp ShoppingCart.cpp POS.cpp DatabaseManager.cpp OperatorAccountRepository.cpp ProductRepository.cpp InventoryRepository.cpp TransactionRepository.cpp main.cpp -lsqlite3 -lbcrypt -o pos

# For non-Windows builds, replace -lbcrypt with -lcrypto and use the platform's
# SQLite include and library paths.

# Test suite (Qt GUI smoke tests run through CMake/CTest)
g++ -std=c++17 -Wall -I. -Itests tests/test_*.cpp Product.cpp CartItem.cpp ShoppingCart.cpp POS.cpp DatabaseManager.cpp OperatorAccountRepository.cpp ProductRepository.cpp InventoryRepository.cpp TransactionRepository.cpp gui/PosController.cpp -lsqlite3 -lbcrypt -o tests/run_tests.exe
```

---

## Running the Application

```bash
# After building
./pos          # Linux / Git Bash
pos.exe        # Windows PowerShell / CMD
```

On first launch, create the administrator account when prompted. On later
launches, log in with that account or an operator account created by an
administrator. The desktop app offers a **Log out** control; the console menu
also offers logout.

The program displays a numbered main menu:

| Option | Action |
|--------|--------|
| 1 | View product catalog |
| 2 | Add a product to the cart |
| 3 | View current cart |
| 4 | Remove a product from the cart |
| 5 | Update a product's quantity |
| 6 | Checkout and print receipt |
| 7 | Exit |
| 8 | Product management |
| 9 | Inventory management |
| 10 | Transaction history and sales reports |
| 11 | Logout |

---

## Running the Test Suite

```bash
make test
# or manually:
./tests/run_tests.exe
```

The Makefile suite uses **doctest** for unit, property-style, and integration
tests. Qt GUI smoke tests run through CMake/CTest with the offscreen platform.
Property-style tests use deterministic random or bounded generated cases and
include at least 100 iterations where applicable. Property tests reference
their design-document property with a comment of the form:

```cpp
// Feature: bakery-pos-system, Property N: <property text>
```

---

## Product Catalog

| Product ID | Name | Unit Price |
|------------|------|------------|
| P001 | Chocolate Chip Cookie | PHP 65.00 |
| P002 | Double Chocolate Cookie | PHP 70.00 |
| P003 | Red Velvet Cookie | PHP 70.00 |
| P004 | S'mores Cookie | PHP 75.00 |
| P005 | Brookies | PHP 75.00 |
| P006 | Nutella | PHP 65.00 |
| P007 | Dubai Chewy Cookie (1 Piece) | PHP 110.00 |
| P008 | Dubai Chewy Cookie (4 Pieces) | PHP 420.00 |

---

The initial stock for each seeded product is 100. The two Dubai Chewy Cookie
sizes are separate product records with independent stock. Existing product
management can edit each product through the console Product Management menu;
seeding does not reset those edits on later starts.

## Notes

- All monetary values are formatted as `PHP X.XX` (two decimal places).
- The cart is cleared after every successful checkout.
- Transaction IDs are assigned by SQLite and remain persistent across restarts.
- Invalid input is always caught and reported; the application never crashes on bad input.
