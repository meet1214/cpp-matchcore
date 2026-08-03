#pragma once
#include "Order.h"
#include "Trade.h"
#include <cstdint>
#include <functional>
#include <map>
#include <deque>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <optional>

class OrderBook {
    public:
        std::vector<Trade> addOrder(Order order);
        bool cancelOrder(uint64_t orderId);
        std::optional<double> bestBid() const;
        std::optional<double> bestAsk() const;
        uint64_t bidDepth() const;
        uint64_t askDepth() const;
        std::vector<Order> snapshot() const;
        void restoreOrder(const Order& order);
    
    private:
        struct Location{ double price; Side side; };
        std::map<double, std::deque<Order>, std::greater<double>> bids_;
        std::map<double, std::deque<Order>, std::less<double>> asks_;
        std::unordered_map<uint64_t, Location> orderIndex_;
        mutable std::mutex mutex_;
        uint64_t nextOrderSequence_ = 0;
        uint64_t nextTradeSequence_ = 0;
        std::vector<Trade> matchAgainstAsks(Order& incoming);
        std::vector<Trade> matchAgainstBids(Order& incoming);
};