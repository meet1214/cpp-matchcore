#pragma once
#include "OrderBook.h"
#include "ThreadPool.h"
#include <atomic>
#include <cstdint>
#include <string>

class Server {
public:
    Server(int port, std::size_t threadCount);
    void run(); // blocks, loops accepting clients forever

private:
    int port_;
    OrderBook book_;
    ThreadPool pool_;
    std::atomic<uint64_t> nextOrderId_{1};

    void handleClient(int clientFd);
    void handleLine(const std::string& line, int clientFd);
};