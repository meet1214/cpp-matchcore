#pragma once
#include "OrderBook.h"
#include <cstdint>
#include <string>

class Server {
public:
    explicit Server(int port);
    void run(); // blocks, handles one client

private:
    int port_;
    OrderBook book_;
    uint64_t nextOrderId_ = 1;

    void handleLine(const std::string& line, int clientFd);
};