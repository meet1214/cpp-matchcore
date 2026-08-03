#include "OrderBook.h"
#include <algorithm>
#include <mutex>
#include <vector>

std::vector<Trade> OrderBook::addOrder(Order order, TimeInForce tif) {
    std::lock_guard<std::mutex> lock(mutex_);

    order.sequence = nextOrderSequence_++;

    std::vector<Trade> trades;
    if(order.side == Side::Buy) {
        trades = matchAgainstAsks(order);
    } else {
        trades = matchAgainstBids(order);
    }

    if(order.quantity > 0 && tif == TimeInForce::GTC) {
        if(order.side == Side::Buy) {
            bids_[order.price].push_back(order);
        } else {
            asks_[order.price].push_back(order);
        }
        orderIndex_[order.id] = Location{order.price, order.side};
    }

    return trades;
}

std::vector<Trade> OrderBook::matchAgainstAsks(Order& incoming) {
    std::vector<Trade> trades;

    while(incoming.quantity > 0 && !asks_.empty()) {
        auto bestLevel = asks_.begin();

        if(incoming.price < bestLevel->first) {
            break;
        }
        auto &dq = bestLevel->second;
        Order& resting = dq.front();

        uint64_t matchQty = std::min(incoming.quantity,resting.quantity);

        Trade trade{incoming.id, resting.id, resting.price, matchQty, nextTradeSequence_++};
        trades.push_back(trade);

        incoming.quantity -= matchQty;
        resting.quantity -= matchQty;

        if(resting.quantity == 0) {
            orderIndex_.erase(resting.id);
            dq.pop_front();
            if(dq.empty()) {
                asks_.erase(bestLevel);
            }
        }
    }

    return trades;
}

std::vector<Trade> OrderBook::matchAgainstBids(Order& incoming) {
    std::vector<Trade> trades;

    while (incoming.quantity > 0 && !bids_.empty()) {
        auto bestLevel = bids_.begin();

        if (incoming.price > bestLevel->first) {
            break;
        }

        auto& dq = bestLevel->second;
        Order& resting = dq.front();

        uint64_t matchQty = std::min(incoming.quantity, resting.quantity);

        Trade trade{resting.id, incoming.id, resting.price, matchQty, nextTradeSequence_++};
        trades.push_back(trade);

        incoming.quantity -= matchQty;
        resting.quantity -= matchQty;

        if (resting.quantity == 0) {
            orderIndex_.erase(resting.id);
            dq.pop_front();
            if (dq.empty()) {
                bids_.erase(bestLevel);
            }
        }
    }

    return trades;
}

bool OrderBook::cancelOrder(uint64_t orderId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = orderIndex_.find(orderId);
    if (it == orderIndex_.end()) {
        return false;
    }

    Location loc = it->second;

    if(loc.side == Side::Buy) {
        auto levelIt = bids_.find(loc.price);
        auto &dq = levelIt->second;
        auto orderIt = std::find_if(dq.begin(),dq.end(), 
          [orderId](const Order& o) {return o.id ==orderId; });
        dq.erase(orderIt);
        if(dq.empty()) {
            bids_.erase(levelIt);
        }

    } else {
        auto levelIt = asks_.find(loc.price);
        auto& dq = levelIt->second;
        auto orderIt = std::find_if(dq.begin(), dq.end(),
            [orderId](const Order& o) { return o.id == orderId; });
        dq.erase(orderIt);
        if (dq.empty()) {
            asks_.erase(levelIt);
        }
    }

    orderIndex_.erase(it);
    return true;
}

std::optional<double> OrderBook::bestBid() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (bids_.empty()) {
        return std::nullopt;
    }
    return bids_.begin()->first;
}

std::optional<double> OrderBook::bestAsk() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (asks_.empty()) {
        return std::nullopt;
    }
    return asks_.begin()->first;
}

uint64_t OrderBook::bidDepth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t total = 0;
    for (const auto& [price, dq] : bids_) {
        for (const auto& order : dq) {
            total += order.quantity;
        }
    }
    return total;
}

uint64_t OrderBook::askDepth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t total = 0;
    for (const auto& [price, dq] : asks_) {
        for (const auto& order : dq) {
            total += order.quantity;
        }
    }
    return total;
}

std::vector<Order> OrderBook::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Order> result;
    for (const auto& [price, dq] : bids_) {
        for (const auto& o : dq) result.push_back(o);
    }
    for (const auto& [price, dq] : asks_) {
        for (const auto& o : dq) result.push_back(o);
    }
    return result;
}

void OrderBook::restoreOrder(const Order& order) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (order.side == Side::Buy) {
        bids_[order.price].push_back(order);
    } else {
        asks_[order.price].push_back(order);
    }
    orderIndex_[order.id] = Location{order.price, order.side};
    if (order.sequence >= nextOrderSequence_) {
        nextOrderSequence_ = order.sequence + 1;
    }
}