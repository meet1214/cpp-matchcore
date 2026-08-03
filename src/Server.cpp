#include "Server.h"
#include "OrderBook.h"
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

namespace {
void sendAllLocked(int fd, const std::string& data, std::mutex& writeMutex) {
    std::lock_guard<std::mutex> lock(writeMutex);
    size_t sent = 0;
    while (sent < data.size()) {
        ssize_t n = write(fd, data.c_str() + sent, data.size() - sent);
        if (n <= 0) return;
        sent += n;
    }
}
}

Server::Server(int port, std::size_t threadCount, const std::string& dbPath)
    : port_(port), pool_(threadCount), logger_(dbPath), userStore_(dbPath), orderStore_(dbPath) {
    auto restored = orderStore_.loadAll();
    for (auto& [symbol, orders] : restored) {
        OrderBook& book = getBook(symbol);
        for (const auto& o : orders) book.restoreOrder(o);
    }
    std::cout << "Restored resting orders for " << restored.size() << " symbol(s).\n";
}

OrderBook& Server::getBook(const std::string& symbol) {
    std::lock_guard<std::mutex> lock(booksMutex_);
    return books_[symbol];
}

void Server::broadcast(const std::string& symbol, const Trade& t) {
    std::string msg = "[MARKET] " + symbol + " traded " + std::to_string(t.quantity) +
                       " @ " + std::to_string(t.price) + "\n";

    std::lock_guard<std::mutex> lock(clientsMutex_);
    for (auto& [fd, mtx] : clientWriteMutexes_) {
        sendAllLocked(fd, msg, *mtx);
    }
}

void Server::run() {
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) { perror("socket failed"); return; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listenFd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed"); close(listenFd_); return;
    }
    if (listen(listenFd_, 16) < 0) {
        perror("listen failed"); close(listenFd_); return;
    }

    std::cout << "Listening on port " << port_ << "\n";

    while (!stopping_) {
        int clientFd = accept(listenFd_, nullptr, nullptr);
        if (stopping_) break;
        if (clientFd < 0) { perror("accept failed"); continue; }
        std::cout << "Client connected: fd=" << clientFd << "\n";
        pool_.submit([this, clientFd] { handleClient(clientFd); });
    }

    std::cout << "Server stopped.\n";
}

void Server::stop() {
    stopping_ = true;
    if (listenFd_ >= 0) {
        close(listenFd_);
    }
}

void Server::handleClient(int clientFd) {
    ClientSession session;
    session.writeMutex = std::make_shared<std::mutex>();

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clientWriteMutexes_[clientFd] = session.writeMutex;
    }

    sendAllLocked(clientFd, "Welcome to MatchCore. Type HELP for commands.\n", *session.writeMutex);

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
            handleLine(line, clientFd, session);
        }
    }

    {
        std::lock_guard<std::mutex> lock(clientsMutex_);
        clientWriteMutexes_.erase(clientFd);
    }

    close(clientFd);
    std::cout << "Client disconnected: fd=" << clientFd << "\n";
}

void Server::handleLine(const std::string& line, int clientFd, ClientSession& session) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - session.windowStart).count();
    if (elapsed >= 1) {
        session.requestCount = 0;
        session.windowStart = now;
    }
    session.requestCount++;
    if (session.requestCount > 10) {
        sendAllLocked(clientFd, "Rate limit exceeded. Slow down.\n", *session.writeMutex);
        return;
    }

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    std::ostringstream response;

    if (cmd == "BUY" || cmd == "SELL") {
        if (!session.authenticated) {
            response << "You must log in before placing orders. Use the login menu.\n";
        } else {
            std::string symbol;
            double price;
            uint64_t qty;
            iss >> symbol >> price >> qty;

            if (iss.fail() || symbol.empty() || price <= 0 || qty == 0) {
                response << "Invalid order. Usage: BUY <symbol> <price> <qty> [IOC]\n";
            } else {
                std::string tifToken;
                iss >> tifToken;
                TimeInForce tif = (tifToken == "IOC") ? TimeInForce::IOC : TimeInForce::GTC;

                Order order{nextOrderId_++, session.clientId, cmd == "BUY" ? Side::Buy : Side::Sell, price, qty, 0};
                OrderBook& book = getBook(symbol);
                auto trades = book.addOrder(order, tif);
                for (auto& t : trades) {
                    logger_.log(symbol, t);
                    broadcast(symbol, t);
                }
                orderStore_.save(symbol, book.snapshot());

                uint64_t filled = 0;
                for (auto& t : trades) filled += t.quantity;
                uint64_t remaining = qty - filled;

                response << "Order #" << order.id << " placed: " << cmd << " " << symbol
                          << " " << qty << " @ " << price;
                if (tif == TimeInForce::IOC) response << " (IOC)";
                response << "\n";

                for (auto& t : trades) {
                    response << "  -> Matched " << t.quantity << " @ " << t.price << "\n";
                }
                if (remaining > 0) {
                    if (tif == TimeInForce::IOC) {
                        response << "  -> " << remaining << " cancelled (IOC, not resting)\n";
                    } else {
                        response << "  -> " << remaining << " resting on the book\n";
                    }
                } else if (!trades.empty()) {
                    response << "  -> Fully filled\n";
                }
            }
        }
    } else if (cmd == "LOGIN") {
        std::string accountNumber, password;
        iss >> accountNumber >> password;
        if (iss.fail()) {
            response << "Usage: LOGIN <accountNumber> <password>\n";
        } else {
            uint64_t clientId;
            if (userStore_.verifyUser(accountNumber, password, clientId)) {
                session.authenticated = true;
                session.clientId = clientId;
                response << "Welcome back! You're now logged in.\n";
            } else {
                response << "Login failed: invalid account number or password.\n";
            }
        }
    } else if (cmd == "REGISTER") {
        std::string password;
        iss >> password;
        std::string name;
        std::getline(iss, name);
        if (!name.empty() && name[0] == ' ') name.erase(0, 1);

        if (password.empty() || name.empty()) {
            response << "Usage: REGISTER <password> <full name>\n";
        } else {
            std::string accountNumber;
            if (userStore_.registerUser(name, password, accountNumber)) {
                response << "Account created for " << name << ". Your account number is "
                          << accountNumber << " — save it, you'll need it to log in.\n";
            } else {
                response << "Registration failed.\n";
            }
        }
    } else if (cmd == "CANCEL") {
        std::string symbol;
        uint64_t id;
        iss >> symbol >> id;
        if (!session.authenticated) {
            response << "You must log in before cancelling orders.\n";
        } else if (iss.fail() || symbol.empty()) {
            response << "Usage: CANCEL <symbol> <orderId>\n";
        } else {
            OrderBook& book = getBook(symbol);
            bool ok = book.cancelOrder(id);
            if (ok) orderStore_.save(symbol, book.snapshot());
            response << (ok ? "Order #" + std::to_string(id) + " cancelled.\n"
                            : "No resting order found with ID " + std::to_string(id) + " for " + symbol + ".\n");
        }
    } else if (cmd == "BOOK") {
        std::string symbol;
        iss >> symbol;
        if (symbol.empty()) {
            std::lock_guard<std::mutex> lock(booksMutex_);
            if (books_.empty()) {
                response << "No symbols traded yet.\n";
            } else {
                response << "----- Active Symbols -----\n";
                for (auto& [sym, book] : books_) {
                    auto bid = book.bestBid();
                    auto ask = book.bestAsk();
                    response << sym << ": bid=" << (bid ? std::to_string(*bid) : "none")
                              << " ask=" << (ask ? std::to_string(*ask) : "none") << "\n";
                }
                response << "---------------------------\n";
            }
        } else {
            OrderBook& book = getBook(symbol);
            auto bid = book.bestBid();
            auto ask = book.bestAsk();
            response << "----- " << symbol << " Order Book -----\n"
                      << "Best Bid: " << (bid ? std::to_string(*bid) : "none")
                      << "  (depth: " << book.bidDepth() << ")\n"
                      << "Best Ask: " << (ask ? std::to_string(*ask) : "none")
                      << "  (depth: " << book.askDepth() << ")\n"
                      << "-----------------------\n";
        }
    } else if (cmd == "HELP") {
        response << "Commands:\n"
                  << "  REGISTER <password> <full name>\n"
                  << "  LOGIN <accountNumber> <password>\n"
                  << "  BUY <symbol> <price> <qty> [IOC]\n"
                  << "  SELL <symbol> <price> <qty> [IOC]\n"
                  << "  CANCEL <symbol> <orderId>\n"
                  << "  BOOK [symbol]\n"
                  << "  HELP\n";
    } else if (cmd == "QUIT") {
        response << "Goodbye!\n";
    } else {
        response << "Unknown command '" << cmd << "'. Type HELP to see available commands.\n";
    }

    sendAllLocked(clientFd, response.str(), *session.writeMutex);
}