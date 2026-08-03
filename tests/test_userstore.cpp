#include "UserStore.h"
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
    const std::string dbPath = "test_userstore.db";
    std::remove(dbPath.c_str());

    {
        UserStore store(dbPath);

        std::string acct1;
        bool reg1 = store.registerUser("Alice Smith", "secret123", acct1);
        check(reg1, "registering a new user succeeds");
        check(acct1.substr(0, 2) == "MC", "account number has MC prefix");

        std::string acct2;
        bool reg2 = store.registerUser("Bob Jones", "hunter2", acct2);
        check(reg2, "registering a second user succeeds");
        check(acct1 != acct2, "two users get different account numbers");

        uint64_t clientId;
        check(store.verifyUser(acct1, "secret123", clientId), "correct password verifies successfully");
        check(!store.verifyUser(acct1, "wrongpassword", clientId), "wrong password fails verification");
        check(!store.verifyUser("MC9999999999", "secret123", clientId), "unknown account number fails verification");
    }

    std::remove(dbPath.c_str());

    std::cout << "\n" << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}