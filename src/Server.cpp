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
    : port_(port), pool_(threadCount), logger_(dbPath), userStore_(dbPath), orderStore_(dbPath) {
    for (const auto& o : orderStore_.loadAll()) {
        book_.restoreOrder(o);
    }
    std::cout << "Restored " << (book_.bidDepth() + book_.askDepth()) << " units of resting orders.\n";
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
        sendAll(clientFd, "Welcome to MatchCore. Type HELP for commands.\n");
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
    close(clientFd);
    std::cout << "Client disconnected: fd=" << clientFd << "\n";
}

void Server::handleLine(const std::string& line, int clientFd, ClientSession& session) {
    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;
    std::ostringstream response;

    if (cmd == "BUY" || cmd == "SELL") {
        if (!session.authenticated) {
            response << "You must log in before placing orders. Use the login menu.\n";
        } else {
            double price;
            uint64_t qty;
            iss >> price >> qty;
            if (iss.fail() || price <= 0 || qty == 0) {
                response << "Invalid order. Usage: BUY <price> <qty> or SELL <price> <qty>, both positive.\n";
            } else {
                Order order{nextOrderId_++, session.clientId, cmd == "BUY" ? Side::Buy : Side::Sell, price, qty, 0};
                auto trades = book_.addOrder(order);
                for (auto& t : trades) logger_.log(t);
                orderStore_.save(book_.snapshot());

                uint64_t filled = 0;
                for (auto& t : trades) filled += t.quantity;
                uint64_t remaining = qty - filled;

                response << "Order #" << order.id << " placed: " << cmd << " " << qty << " @ " << price << "\n";
                for (auto& t : trades) {
                    response << "  -> Matched " << t.quantity << " @ " << t.price << "\n";
                }
                if (remaining > 0) {
                    response << "  -> " << remaining << " resting on the book\n";
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
        uint64_t id; iss >> id;
        if (!session.authenticated) {
            response << "You must log in before cancelling orders.\n";
        } else {
            bool ok = book_.cancelOrder(id);
            if (ok) orderStore_.save(book_.snapshot());
            response << (ok ? "Order #" + std::to_string(id) + " cancelled.\n"
                            : "No resting order found with ID " + std::to_string(id) + ".\n");
        }
    } else if (cmd == "BOOK") {
        auto bid = book_.bestBid();
        auto ask = book_.bestAsk();
        response << "----- Order Book -----\n"
                  << "Best Bid: " << (bid ? std::to_string(*bid) : "none")
                  << "  (depth: " << book_.bidDepth() << ")\n"
                  << "Best Ask: " << (ask ? std::to_string(*ask) : "none")
                  << "  (depth: " << book_.askDepth() << ")\n"
                  << "-----------------------\n";
    } else if (cmd == "HELP") {
        response << "Commands:\n"
                  << "  REGISTER <password> <full name>\n"
                  << "  LOGIN <accountNumber> <password>\n"
                  << "  BUY <price> <qty>\n"
                  << "  SELL <price> <qty>\n"
                  << "  CANCEL <orderId>\n"
                  << "  BOOK\n"
                  << "  HELP\n";
    } else if (cmd == "QUIT") {
    response << "Goodbye!\n";
    
    } else {
        response << "Unknown command '" << cmd << "'. Type HELP to see available commands.\n";
    }

    sendAll(clientFd, response.str());
}