#include "TradeLogger.h"
#include <sqlite3.h>
#include <iostream>
#include <cstdio>
#include <string>

int failures = 0;

void check(bool condition, const std::string& testName) {
    if (condition) {
        std::cout << "[PASS] " << testName << "\n";
    } else {
        std::cout << "[FAIL] " << testName << "\n";
        failures++;
    }
}

int main() {
    const std::string dbPath = "test_tradelogger.db";
    std::remove(dbPath.c_str());

    {
        TradeLogger logger(dbPath);
        logger.log("AAPL", Trade{1, 2, 50.0, 10, 0});
        logger.log("GOOG", Trade{3, 4, 51.5, 5, 1});
    }

    sqlite3* db;
    sqlite3_open(dbPath.c_str(), &db);

    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM trades;", -1, &stmt, nullptr);
    sqlite3_step(stmt);
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    check(count == 2, "both logged trades were persisted");

    sqlite3_prepare_v2(db, "SELECT price, quantity FROM trades WHERE symbol = 'AAPL' AND sequence = 0;", -1, &stmt, nullptr);    sqlite3_step(stmt);
    double price = sqlite3_column_double(stmt, 0);
    int qty = sqlite3_column_int(stmt, 1);
    sqlite3_finalize(stmt);
    check(price == 50.0 && qty == 10, "first trade's fields stored correctly");

    sqlite3_close(db);
    std::remove(dbPath.c_str());

    std::cout << "\n" << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}