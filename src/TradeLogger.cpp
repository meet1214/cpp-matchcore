#include "TradeLogger.h"
#include <stdexcept>
#include <iostream>

TradeLogger::TradeLogger(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open DB: " + std::string(sqlite3_errmsg(db_)));
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_busy_timeout(db_, 5000); // retry for up to 5 seconds

    const char* createSql =
        "CREATE TABLE IF NOT EXISTS trades ("
        "sequence INTEGER PRIMARY KEY,"
        "buy_order_id INTEGER NOT NULL,"
        "sell_order_id INTEGER NOT NULL,"
        "price REAL NOT NULL,"
        "quantity INTEGER NOT NULL,"
        "created_at TEXT DEFAULT CURRENT_TIMESTAMP);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, createSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to create table: " + err);
    }
}

TradeLogger::~TradeLogger() {
    sqlite3_close(db_);
}

void TradeLogger::log(const Trade& trade) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql =
        "INSERT INTO trades (sequence, buy_order_id, sell_order_id, price, quantity) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Prepare failed: " << sqlite3_errmsg(db_) << "\n";
        return;
    }

    sqlite3_bind_int64(stmt, 1, trade.sequence);
    sqlite3_bind_int64(stmt, 2, trade.buyOrderId);
    sqlite3_bind_int64(stmt, 3, trade.sellOrderId);
    sqlite3_bind_double(stmt, 4, trade.price);
    sqlite3_bind_int64(stmt, 5, trade.quantity);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        std::cerr << "Insert failed: " << sqlite3_errmsg(db_) << "\n";
    }

    sqlite3_finalize(stmt);
}