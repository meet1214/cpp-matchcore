cmake_minimum_required(VERSION 3.16)
project(MatchCore VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SQLite3 REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(matchcore_demo
    src/main.cpp
    src/OrderBook.cpp
)
target_include_directories(matchcore_demo PRIVATE include)

add_executable(matchcore_tests
    tests/test_orderbook.cpp
    src/OrderBook.cpp
)
target_include_directories(matchcore_tests PRIVATE include)

add_executable(matchcore_server
    src/main_server.cpp
    src/Server.cpp
    src/OrderBook.cpp
    src/TradeLogger.cpp
    src/UserStore.cpp
    src/OrderStore.cpp
)
target_include_directories(matchcore_server PRIVATE include)
target_link_libraries(matchcore_server PRIVATE SQLite3::SQLite3 OpenSSL::Crypto)
add_executable(matchcore_client src/Client.cpp)
target_include_directories(matchcore_client PRIVATE include)

add_executable(matchcore_userstore_tests
    tests/test_userstore.cpp
    src/UserStore.cpp
)

target_include_directories(matchcore_userstore_tests PRIVATE include)
target_link_libraries(matchcore_userstore_tests PRIVATE SQLite3::SQLite3 OpenSSL::Crypto)

add_executable(matchcore_tradelogger_tests
    tests/test_tradelogger.cpp
    src/TradeLogger.cpp
)
target_include_directories(matchcore_tradelogger_tests PRIVATE include)
target_link_libraries(matchcore_tradelogger_tests PRIVATE SQLite3::SQLite3)