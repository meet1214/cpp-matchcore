#pragma once
#include "OrderBook.h"
#include "ThreadPool.h"
#include "TradeLogger.h"
#include "UserStore.h"
#include "OrderStore.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <chrono>

struct ClientSession {
    bool authenticated = false;
    uint64_t clientId = 0;
    int requestCount = 0;
    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
};

class Server {
public:
    Server(int port, std::size_t threadCount, const std::string& dbPath);
    void run();
    void stop();

private:
    int port_;
    int listenFd_ = -1;
    std::atomic<bool> stopping_{false};
    OrderBook book_;
    ThreadPool pool_;
    TradeLogger logger_;
    UserStore userStore_;
    OrderStore orderStore_;
    std::atomic<uint64_t> nextOrderId_{1};

    void handleClient(int clientFd);
    void handleLine(const std::string& line, int clientFd, ClientSession& session);
};