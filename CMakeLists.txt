cmake_minimum_required(VERSION 3.25.1)
project(arenda
        VERSION 0.4.0.0
        LANGUAGES CXX
        HOMEPAGE_URL "https://github.com/khromenokroman/arenda")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Собирать тесты" ON)

find_package(SQLite3 REQUIRED)
find_package(nlohmann_json 3 REQUIRED)
find_package(Threads REQUIRED)

include(GNUInstallDirs)

# cpp-httplib header-only из third_party: пакет Debian собран в разделяемую библиотеку без .a
add_library(httplib INTERFACE)
target_include_directories(httplib SYSTEM INTERFACE third_party)

set(ARENDA_WEB_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/web")

add_library(${PROJECT_NAME}-core STATIC
        src/api.cpp
        src/card_store.cpp
        src/dates.cpp
        src/db.cpp
        src/fields.cpp
        src/json_io.cpp
        src/landlords.cpp
        src/names.cpp
        src/options.cpp
        src/requisites.cpp
        src/schema.cpp
        src/tenants.cpp
)
target_include_directories(${PROJECT_NAME}-core PUBLIC src)
target_compile_definitions(${PROJECT_NAME}-core PUBLIC
        ARENDA_VERSION="${PROJECT_VERSION}"
        ARENDA_WEB_DIR="${ARENDA_WEB_DIR}")
target_compile_options(${PROJECT_NAME}-core PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(${PROJECT_NAME}-core PUBLIC
        httplib SQLite::SQLite3 nlohmann_json::nlohmann_json Threads::Threads)

add_executable(${PROJECT_NAME}
        src/main.cpp
)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}-core)

if (BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    add_executable(${PROJECT_NAME}-tests
            tests/api_test.cpp
            tests/dates_test.cpp
            tests/db_test.cpp
            tests/landlords_test.cpp
            tests/names_test.cpp
            tests/options_test.cpp
            tests/requisites_test.cpp
            tests/schema_test.cpp
            tests/tenants_test.cpp
    )
    target_link_libraries(${PROJECT_NAME}-tests PRIVATE ${PROJECT_NAME}-core GTest::gtest_main GTest::gmock)
    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}-tests)
endif ()

install(TARGETS ${PROJECT_NAME}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY web/
        DESTINATION ${ARENDA_WEB_DIR}
)
