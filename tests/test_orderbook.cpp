#include "OrderBook.h"
#include <iostream>
#include <string>

int failures = 0;

void check(bool condition, const std::string& testName) {
    if (condition) {
        std::cout << "[PASS] " << testName << "\n";
    } else {
        std::cout << "[FAIL] " << testName << "\n";
        failures++;
    }
}

void testFullMatchWithPriceImprovement() {
    OrderBook book;
    book.addOrder(Order{1, 100, Side::Sell, 50.0, 10, 0});
    auto trades = book.addOrder(Order{2, 200, Side::Buy, 55.0, 10, 0});

    check(trades.size() == 1, "full match produces exactly one trade");
    check(trades[0].price == 50.0, "trade executes at resting price (price improvement)");
    check(trades[0].quantity == 10, "trade fills the full quantity");
    check(!book.bestAsk().has_value(), "ask side is empty after a full match");
}

void testPartialFill() {
    OrderBook book;
    book.addOrder(Order{1, 100, Side::Sell, 60.0, 5, 0});
    auto trades = book.addOrder(Order{2, 200, Side::Buy, 60.0, 8, 0});

    check(trades.size() == 1, "partial fill produces one trade");
    check(trades[0].quantity == 5, "trade fills only the available resting quantity");
    check(book.bidDepth() == 3, "unfilled remainder rests on the book");
}

void testTimePriority() {
    OrderBook book;
    book.addOrder(Order{1, 100, Side::Sell, 70.0, 4, 0});
    book.addOrder(Order{2, 100, Side::Sell, 70.0, 4, 0});
    auto trades = book.addOrder(Order{3, 200, Side::Buy, 70.0, 4, 0});

    check(trades[0].sellOrderId == 1, "earlier resting order at the same price fills first");
}

void testNonCrossingOrders() {
    OrderBook book;
    book.addOrder(Order{1, 100, Side::Buy, 40.0, 5, 0});
    auto trades = book.addOrder(Order{2, 200, Side::Sell, 45.0, 5, 0});

    check(trades.empty(), "non-crossing orders produce no trades");
    check(book.bestBid().has_value() && *book.bestBid() == 40.0, "buy order rests at its own price");
    check(book.bestAsk().has_value() && *book.bestAsk() == 45.0, "sell order rests at its own price");
}

void testCancelOrder() {
    OrderBook book;
    book.addOrder(Order{1, 100, Side::Sell, 80.0, 6, 0});
    bool cancelled = book.cancelOrder(1);
    auto trades = book.addOrder(Order{2, 200, Side::Buy, 80.0, 6, 0});

    check(cancelled, "cancelOrder reports success for an order that exists");
    check(trades.empty(), "a matching order finds nothing after the resting order is cancelled");
}

int main() {
    testFullMatchWithPriceImprovement();
    testPartialFill();
    testTimePriority();
    testNonCrossingOrders();
    testCancelOrder();

    std::cout << "\n" << failures << " failure(s)\n";
    return failures == 0 ? 0 : 1;
}