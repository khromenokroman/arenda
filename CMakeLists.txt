cmake_minimum_required(VERSION 3.25.1)
project(arenda
        VERSION 0.9.1.0
        LANGUAGES CXX
        HOMEPAGE_URL "https://github.com/khromenokroman/arenda")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Собирать тесты" ON)

find_package(SQLite3 REQUIRED)
find_package(nlohmann_json 3 REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include(GNUInstallDirs)

# cpp-httplib header-only из third_party: пакет Debian собран в разделяемую библиотеку без .a.
# HTTPS (запросы к DaData) через OpenSSL; макрос должен быть одинаковым во всех единицах трансляции.
add_library(httplib INTERFACE)
target_include_directories(httplib SYSTEM INTERFACE third_party)
target_compile_definitions(httplib INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
target_link_libraries(httplib INTERFACE OpenSSL::SSL OpenSSL::Crypto)

# шаблон договора вшивается в программу: правка templates/contract.html пересобирает её
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/templates/contract.html ARENDA_CONTRACT_TEMPLATE)
configure_file(src/contract_template.cpp.in contract_template.cpp @ONLY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS templates/contract.html)

set(ARENDA_WEB_DIR "${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/web")

add_library(${PROJECT_NAME}-core STATIC
        src/api.cpp
        src/card_store.cpp
        src/contract_print.cpp
        ${CMAKE_CURRENT_BINARY_DIR}/contract_template.cpp
        src/contracts.cpp
        src/dadata.cpp
        src/dates.cpp
        src/db.cpp
        src/equipment.cpp
        src/fields.cpp
        src/json_io.cpp
        src/landlords.cpp
        src/lease_split.cpp
        src/money_words.cpp
        src/names.cpp
        src/numbers.cpp
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
            tests/contract_print_test.cpp
            tests/contracts_test.cpp
            tests/dadata_test.cpp
            tests/dates_test.cpp
            tests/db_test.cpp
            tests/equipment_test.cpp
            tests/landlords_test.cpp
            tests/lease_split_test.cpp
            tests/money_words_test.cpp
            tests/names_test.cpp
            tests/numbers_test.cpp
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
install(FILES debian/copyright
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

# служба systemd: пользователь arenda, база в /var/lib/arenda, параметры и ключ DaData в /etc/default/arenda
configure_file(debian/arenda.service.in arenda.service @ONLY)
configure_file(debian/conffiles.in conffiles @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/arenda.service
        DESTINATION lib/systemd/system
)
install(FILES debian/default
        DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/default
        RENAME arenda
)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "khromenokroman")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "khromenokroman <roma55592@yandex.ru>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Non-residential premises lease contracts with web interface")
set(CPACK_PACKAGE_DESCRIPTION "Landlords and tenants (sole proprietors, companies, individuals) with requisites\nlookup by INN, lease term split into several contracts, printable contracts.")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")

set(CPACK_GENERATOR "DEB")
set(CPACK_STRIP_FILES ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "misc")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# ca-certificates: проверка сертификата DaData по HTTPS
set(CPACK_DEBIAN_PACKAGE_DEPENDS "adduser, init-system-helpers, ca-certificates")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        ${CMAKE_CURRENT_BINARY_DIR}/conffiles
        ${CMAKE_CURRENT_SOURCE_DIR}/debian/postinst
        ${CMAKE_CURRENT_SOURCE_DIR}/debian/prerm
        ${CMAKE_CURRENT_SOURCE_DIR}/debian/postrm
)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

include(CPack)
