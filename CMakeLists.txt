cmake_minimum_required(VERSION 3.21)

project(CrombiBakeryPOS VERSION 3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.12.0 EXACT REQUIRED COMPONENTS Core Gui Widgets PrintSupport)
find_package(SQLite3 REQUIRED)
if(NOT WIN32)
    find_package(OpenSSL REQUIRED COMPONENTS Crypto)
endif()

qt_add_resources(pos_assets_resources
    assets/assets.qrc
)

set(POS_BUSINESS_SOURCES
    Product.cpp
    CartItem.cpp
    ShoppingCart.cpp
    POS.cpp
    DatabaseManager.cpp
    OperatorAccountRepository.cpp
    ProductRepository.cpp
    InventoryRepository.cpp
    TransactionRepository.cpp
)

add_executable(bakery_pos_console
    main.cpp
    ${POS_BUSINESS_SOURCES}
)
target_link_libraries(bakery_pos_console PRIVATE SQLite3::SQLite3)
if(WIN32)
    target_link_libraries(bakery_pos_console PRIVATE bcrypt)
else()
    target_link_libraries(bakery_pos_console PRIVATE OpenSSL::Crypto)
endif()

add_executable(bakery_pos_gui WIN32
    gui/main.cpp
    gui/AppDataPaths.h
    gui/DashboardPage.cpp
    gui/DashboardPage.h
    gui/MainWindow.cpp
    gui/MainWindow.h
    gui/OperatorManagementDialog.cpp
    gui/OperatorManagementDialog.h
    gui/ProductsPage.cpp
    gui/ProductsPage.h
    gui/InventoryPage.cpp
    gui/InventoryPage.h
    gui/TransactionsPage.cpp
    gui/TransactionsPage.h
    gui/ReportsPage.cpp
    gui/ReportsPage.h
    gui/DisplayFormatting.h
    gui/ExportService.cpp
    gui/ExportService.h
    gui/SalesPage.cpp
    gui/SalesPage.h
    gui/PosController.cpp
    gui/PosController.h
    ${POS_BUSINESS_SOURCES}
)
target_sources(bakery_pos_gui PRIVATE ${pos_assets_resources})
target_link_libraries(bakery_pos_gui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
    SQLite3::SQLite3
)
if(WIN32)
    target_link_libraries(bakery_pos_gui PRIVATE bcrypt)
else()
    target_link_libraries(bakery_pos_gui PRIVATE OpenSSL::Crypto)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(gui_navigation_smoke
        tests/gui_navigation_smoke.cpp
        gui/AppDataPaths.h
        gui/DashboardPage.cpp
        gui/DashboardPage.h
        gui/MainWindow.cpp
        gui/MainWindow.h
        gui/OperatorManagementDialog.cpp
        gui/OperatorManagementDialog.h
        gui/ProductsPage.cpp
        gui/ProductsPage.h
        gui/InventoryPage.cpp
        gui/InventoryPage.h
        gui/TransactionsPage.cpp
        gui/TransactionsPage.h
        gui/ReportsPage.cpp
        gui/ReportsPage.h
        gui/DisplayFormatting.h
        gui/ExportService.cpp
        gui/ExportService.h
        gui/SalesPage.cpp
        gui/SalesPage.h
        gui/PosController.cpp
        gui/PosController.h
        ${POS_BUSINESS_SOURCES}
    )
    target_sources(gui_navigation_smoke PRIVATE ${pos_assets_resources})
    target_link_libraries(gui_navigation_smoke PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
            Qt6::PrintSupport
        SQLite3::SQLite3
    )
    add_test(NAME gui_navigation_smoke COMMAND gui_navigation_smoke)
    if(WIN32)
        target_link_libraries(gui_navigation_smoke PRIVATE bcrypt)
    else()
        target_link_libraries(gui_navigation_smoke PRIVATE OpenSSL::Crypto)
    endif()
    set_tests_properties(gui_navigation_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(gui_sales_smoke
        tests/gui_sales_smoke.cpp
        gui/SalesPage.cpp
        gui/SalesPage.h
        gui/PosController.cpp
        gui/PosController.h
        ${POS_BUSINESS_SOURCES}
    )
    target_sources(gui_sales_smoke PRIVATE ${pos_assets_resources})
    target_link_libraries(gui_sales_smoke PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
            Qt6::PrintSupport
        SQLite3::SQLite3
    )
    add_test(NAME gui_sales_smoke COMMAND gui_sales_smoke)
    if(WIN32)
        target_link_libraries(gui_sales_smoke PRIVATE bcrypt)
    else()
        target_link_libraries(gui_sales_smoke PRIVATE OpenSSL::Crypto)
    endif()
    set_tests_properties(gui_sales_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(gui_products_inventory_smoke
        tests/gui_products_inventory_smoke.cpp
        gui/ProductsPage.cpp
        gui/ProductsPage.h
        gui/InventoryPage.cpp
        gui/InventoryPage.h
        gui/PosController.cpp
        gui/PosController.h
        ${POS_BUSINESS_SOURCES}
    )
    target_sources(gui_products_inventory_smoke PRIVATE ${pos_assets_resources})
    target_link_libraries(gui_products_inventory_smoke PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
            Qt6::PrintSupport
        SQLite3::SQLite3
    )
    add_test(NAME gui_products_inventory_smoke COMMAND gui_products_inventory_smoke)
    if(WIN32)
        target_link_libraries(gui_products_inventory_smoke PRIVATE bcrypt)
    else()
        target_link_libraries(gui_products_inventory_smoke PRIVATE OpenSSL::Crypto)
    endif()
    set_tests_properties(gui_products_inventory_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_executable(gui_transactions_reports_smoke
        tests/gui_transactions_reports_smoke.cpp
        gui/TransactionsPage.cpp
        gui/TransactionsPage.h
        gui/ReportsPage.cpp
        gui/ReportsPage.h
        gui/DisplayFormatting.h
        gui/ExportService.cpp
        gui/ExportService.h
        gui/PosController.cpp
        gui/PosController.h
        ${POS_BUSINESS_SOURCES}
    )
    target_sources(gui_transactions_reports_smoke PRIVATE ${pos_assets_resources})
    target_link_libraries(gui_transactions_reports_smoke PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
            Qt6::PrintSupport
        SQLite3::SQLite3
    )
    add_test(NAME gui_transactions_reports_smoke COMMAND gui_transactions_reports_smoke)
    if(WIN32)
        target_link_libraries(gui_transactions_reports_smoke PRIVATE bcrypt)
    else()
        target_link_libraries(gui_transactions_reports_smoke PRIVATE OpenSSL::Crypto)
    endif()
    set_tests_properties(gui_transactions_reports_smoke PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )

    add_test(NAME gui_transactions_reports_live_database
        COMMAND gui_transactions_reports_smoke
        "${CMAKE_SOURCE_DIR}/bakery_pos.db")
    set_tests_properties(gui_transactions_reports_live_database PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
