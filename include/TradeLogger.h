#pragma once
#include "Trade.h"
#include <sqlite3.h>
#include <mutex>
#include <string>

class TradeLogger {
public:
    explicit TradeLogger(const std::string& dbPath);
    ~TradeLogger();

    void log(const std::string& symbol, const Trade& trade);

private:
    sqlite3* db_;
    std::mutex mutex_;
};