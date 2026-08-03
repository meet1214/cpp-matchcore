#pragma once
#include "Order.h"
#include <sqlite3.h>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

class OrderStore {
public:
    explicit OrderStore(const std::string& dbPath);
    ~OrderStore();

    void save(const std::string& symbol, const std::vector<Order>& orders);
    std::unordered_map<std::string, std::vector<Order>> loadAll();

private:
    sqlite3* db_;
    std::mutex mutex_;
};