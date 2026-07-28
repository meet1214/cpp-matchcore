#include "../include/OrderBook.h"
#include <algorithm>
#include <mutex>
#include <vector>

std::vector<Trade> OrderBook::addOrder(Order order) {
    std::lock_guard<std::mutex> lock(mutex_);

    order.sequence = nextOrderSequence_++;

    std::vector<Trade> trades;
    if(order.side == Side::Buy) {
        trades = matchAgainstAsks(order);
    } else {
        trades = matchAgainstBids(order);
    }

    if(order.quantity > 0) {
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