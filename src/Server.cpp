#include "Server.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace {
void sendAll(int fd, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = write(fd, data.c_str() + sent, data.size() - sent);
        if (n <= 0) return;
        sent += n;
    }
}
}

Server::Server(int port, std::size_t threadCount, const std::string& dbPath)
    : port_(port), pool_(threadCount), logger_(dbPath) {}

void Server::run() {
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) { perror("socket failed"); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed"); close(listenFd); return;
    }
    if (listen(listenFd, 16) < 0) {
        perror("listen failed"); close(listenFd); return;
    }

    std::cout << "Listening on port " << port_ << "\n";

    while (true) {
        int clientFd = accept(listenFd, nullptr, nullptr);
        if (clientFd < 0) { perror("accept failed"); continue; }
        std::cout << "Client connected: fd=" << clientFd << "\n";
        pool_.submit([this, clientFd] { handleClient(clientFd); });
    }
}

void Server::handleClient(int clientFd) {
    char buf[1024];
    std::string leftover;
    while (true) {
        ssize_t n = read(clientFd, buf, sizeof(buf));
        if (n <= 0) break;

        leftover.append(buf, n);
        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string line = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);
            handleLine(line, clientFd);
        }
    }
    close(clientFd);
    std::cout << "Client disconnected: fd=" << clientFd << "\n";
}

void Server::handleLine(const std::string& line, int clientFd) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    std::ostringstream response;

    if (cmd == "BUY" || cmd == "SELL") {
        double price; uint64_t qty;
        iss >> price >> qty;
        Order order{nextOrderId_++, 1, cmd == "BUY" ? Side::Buy : Side::Sell, price, qty, 0};
        auto trades = book_.addOrder(order);
        for (auto& t : trades) logger_.log(t);
        response << "ACCEPTED id=" << order.id << " trades=" << trades.size() << "\n";
        for (auto& t : trades) {
            response << "TRADE price=" << t.price << " qty=" << t.quantity << "\n";
        }
    } else if (cmd == "CANCEL") {
        uint64_t id; iss >> id;
        bool ok = book_.cancelOrder(id);
        response << (ok ? "CANCELLED\n" : "NOT_FOUND\n");
    } else {
        response << "UNKNOWN_COMMAND\n";
    }

    sendAll(clientFd, response.str());
}