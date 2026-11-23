CXX      = g++
SQLITE_INCLUDE = C:/msys64/ucrt64/include
SQLITE_LIB = C:/msys64/ucrt64/lib
CXXFLAGS = -std=c++17 -Wall -Wextra -I. -Itests -I$(SQLITE_INCLUDE)
ifeq ($(OS),Windows_NT)
CRYPTO_LIB = -lbcrypt
else
CRYPTO_LIB = -lcrypto
endif
LIBFLAGS = -L$(SQLITE_LIB) -lsqlite3 $(CRYPTO_LIB)

# Source files for the main application (excludes test runner entry points)
SRCS = Product.cpp CartItem.cpp ShoppingCart.cpp POS.cpp DatabaseManager.cpp OperatorAccountRepository.cpp ProductRepository.cpp InventoryRepository.cpp TransactionRepository.cpp main.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = pos.exe

# Test sources (all .cpp files inside tests/)
TEST_SRCS = $(filter-out tests/gui_navigation_smoke.cpp tests/gui_sales_smoke.cpp tests/gui_products_inventory_smoke.cpp tests/gui_transactions_reports_smoke.cpp,$(wildcard tests/*.cpp))
# When building tests we link the library objects (no main.cpp)
LIB_SRCS = Product.cpp CartItem.cpp ShoppingCart.cpp POS.cpp DatabaseManager.cpp OperatorAccountRepository.cpp ProductRepository.cpp InventoryRepository.cpp TransactionRepository.cpp gui/PosController.cpp
TEST_TARGET = tests/run_tests.exe

.PHONY: all run test clean

## Build the main POS executable
all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) $(LIBFLAGS) -o $(TARGET)

## Run the POS application
run: all
	./$(TARGET)

## Build and run the test suite
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRCS) $(LIB_SRCS)
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(LIB_SRCS) $(LIBFLAGS) -o $(TEST_TARGET)

## Remove build artifacts (Windows-compatible)
clean:
	-del /Q pos.exe 2>nul || true
	-del /Q tests\*.exe 2>nul || true
	-del /Q *.o 2>nul || true
	-del /Q tests\*.db 2>nul || true
