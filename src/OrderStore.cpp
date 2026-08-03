#include "OrderStore.h"
#include <stdexcept>

OrderStore::OrderStore(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open DB: " + std::string(sqlite3_errmsg(db_)));
    }

    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_busy_timeout(db_, 5000);

    const char* createSql =
        "CREATE TABLE IF NOT EXISTS resting_orders ("
        "id INTEGER NOT NULL,"
        "symbol TEXT NOT NULL,"
        "client_id INTEGER NOT NULL,"
        "side INTEGER NOT NULL,"
        "price REAL NOT NULL,"
        "quantity INTEGER NOT NULL,"
        "sequence INTEGER NOT NULL,"
        "PRIMARY KEY (symbol, id));";
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, createSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to create resting_orders table: " + err);
    }
}

OrderStore::~OrderStore() {
    sqlite3_close(db_);
}

void OrderStore::save(const std::string& symbol, const std::vector<Order>& orders) {
    std::lock_guard<std::mutex> lock(mutex_);

    sqlite3_stmt* delStmt;
    sqlite3_prepare_v2(db_, "DELETE FROM resting_orders WHERE symbol = ?;", -1, &delStmt, nullptr);
    sqlite3_bind_text(delStmt, 1, symbol.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(delStmt);
    sqlite3_finalize(delStmt);

    const char* sql =
        "INSERT INTO resting_orders (id, symbol, client_id, side, price, quantity, sequence) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;

    for (const auto& o : orders) {
        sqlite3_bind_int64(stmt, 1, o.id);
        sqlite3_bind_text(stmt, 2, symbol.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, o.clientId);
        sqlite3_bind_int(stmt, 4, o.side == Side::Buy ? 0 : 1);
        sqlite3_bind_double(stmt, 5, o.price);
        sqlite3_bind_int64(stmt, 6, o.quantity);
        sqlite3_bind_int64(stmt, 7, o.sequence);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
}

std::unordered_map<std::string, std::vector<Order>> OrderStore::loadAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::unordered_map<std::string, std::vector<Order>> result;

    const char* sql = "SELECT id, symbol, client_id, side, price, quantity, sequence FROM resting_orders;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Order o;
        o.id = sqlite3_column_int64(stmt, 0);
        std::string symbol = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        o.clientId = sqlite3_column_int64(stmt, 2);
        o.side = sqlite3_column_int(stmt, 3) == 0 ? Side::Buy : Side::Sell;
        o.price = sqlite3_column_double(stmt, 4);
        o.quantity = sqlite3_column_int64(stmt, 5);
        o.sequence = sqlite3_column_int64(stmt, 6);
        result[symbol].push_back(o);
    }
    sqlite3_finalize(stmt);
    return result;
}