#pragma once
#include "OrderBook.h"
#include "ThreadPool.h"
#include "TradeLogger.h"
#include "UserStore.h"
#include "OrderStore.h"
#include "Logger.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

struct ClientSession {
    bool authenticated = false;
    uint64_t clientId = 0;
    int requestCount = 0;
    std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
    std::shared_ptr<std::mutex> writeMutex;
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

    std::unordered_map<std::string, OrderBook> books_;
    std::mutex booksMutex_;

    std::unordered_map<int, std::shared_ptr<std::mutex>> clientWriteMutexes_;
    std::mutex clientsMutex_;

    ThreadPool pool_;
    TradeLogger logger_;
    UserStore userStore_;
    OrderStore orderStore_;
    Logger appLog_;
    std::atomic<uint64_t> nextOrderId_{1};

    OrderBook& getBook(const std::string& symbol);
    void broadcast(const std::string& symbol, const Trade& t);
    void handleClient(int clientFd);
    void handleLine(const std::string& line, int clientFd, ClientSession& session);
};