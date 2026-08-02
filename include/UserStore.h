#pragma once
#include <sqlite3.h>
#include <mutex>
#include <string>
#include <cstdint>

class UserStore {
public:
    explicit UserStore(const std::string& dbPath);
    ~UserStore();

    bool registerUser(const std::string& name, const std::string& password, std::string& outAccountNumber);
    bool verifyUser(const std::string& accountNumber, const std::string& password, uint64_t& outClientId);

private:
    sqlite3* db_;
    std::mutex mutex_;
};