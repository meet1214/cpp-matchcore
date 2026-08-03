#include "UserStore.h"
#include <openssl/sha.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <iostream>

namespace {

std::string generateSalt() {
    std::random_device rd;
    std::stringstream ss;
    for (int i = 0; i < 16; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (rd() % 256);
    }
    return ss.str();
}

std::string sha256Hex(const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), digest);
    std::stringstream ss;
    for (unsigned char b : digest) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return ss.str();
}

bool constantTimeEquals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

std::string formatAccountNumber(int64_t id) {
    std::stringstream ss;
    ss << "MC" << std::setw(10) << std::setfill('0') << id;
    return ss.str();
}

}

UserStore::UserStore(const std::string& dbPath) {
    if (sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK) {
        throw std::runtime_error("Failed to open DB: " + std::string(sqlite3_errmsg(db_)));
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_busy_timeout(db_, 5000); // retry for up to 5 seconds

    const char* createSql =
        "CREATE TABLE IF NOT EXISTS users ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "account_number TEXT UNIQUE,"
        "name TEXT NOT NULL,"
        "salt TEXT NOT NULL,"
        "password_hash TEXT NOT NULL);";

    char* errMsg = nullptr;
    if (sqlite3_exec(db_, createSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string err = errMsg;
        sqlite3_free(errMsg);
        throw std::runtime_error("Failed to create users table: " + err);
    }
}

UserStore::~UserStore() {
    sqlite3_close(db_);
}

bool UserStore::registerUser(const std::string& name, const std::string& password, std::string& outAccountNumber) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string salt = generateSalt();
    std::string hash = sha256Hex(password + salt);

    // Insert with a placeholder account_number first, then update it using the
    // generated id — account_number depends on id, which SQLite only assigns
    // once the row actually exists.
    const char* insertSql = "INSERT INTO users (account_number, name, salt, password_hash) VALUES (NULL, ?, ?, ?);";    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, insertSql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, salt.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, hash.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    int64_t id = sqlite3_last_insert_rowid(db_);
    std::string accountNumber = formatAccountNumber(id);

    const char* updateSql = "UPDATE users SET account_number = ? WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, updateSql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_text(stmt, 1, accountNumber.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, id);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);

    if (ok) outAccountNumber = accountNumber;
    return ok;
}

bool UserStore::verifyUser(const std::string& accountNumber, const std::string& password, uint64_t& outClientId) {
    std::lock_guard<std::mutex> lock(mutex_);

    const char* sql = "SELECT id, salt, password_hash FROM users WHERE account_number = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, accountNumber.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        std::string salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        std::string storedHash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (constantTimeEquals(sha256Hex(password + salt), storedHash)) {
            outClientId = id;
            found = true;
        }
    }
    sqlite3_finalize(stmt);
    return found;
}