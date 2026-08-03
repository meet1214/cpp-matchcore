#pragma once
#include "Order.h"
#include <sqlite3.h>
#include <mutex>
#include <string>
#include <vector>

class OrderStore {
public:
    explicit OrderStore(const std::string& dbPath);
    ~OrderStore();

    void save(const std::vector<Order>& orders);
    std::vector<Order> loadAll();

private:
    sqlite3* db_;
    std::mutex mutex_;
};