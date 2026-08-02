#pragma once
#include "OrderBook.h"
#include "ThreadPool.h"
#include "TradeLogger.h"
#include <atomic>
#include <cstdint>
#include <string>

struct ClientSession {
    bool authenticated = false;
    uint64_t clientId = 0;
};

class Server {
public:
    Server(int port, std::size_t threadCount, const std::string& dbPath);
    void run(); // blocks, loops accepting clients forever

private:
    int port_;
    OrderBook book_;
    ThreadPool pool_;
    TradeLogger logger_;
    std::atomic<uint64_t> nextOrderId_{1};
    std::unordered_map<std::string, std::string> credentials_ = {
    {"alice", "token123"},
    {"bob", "token456"}
    };
    std::unordered_map<std::string, uint64_t> clientIds_ = {
        {"alice", 1},
        {"bob", 2}
    };

    void handleClient(int clientFd);
    void handleLine(const std::string& line, int clientFd, ClientSession& session);
};