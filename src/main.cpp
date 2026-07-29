#include "OrderBook.h"
#include <iostream>

void printTrades(const std::vector<Trade>& trades) {
    for (const auto& t : trades) {
        std::cout << "TRADE  buy#" << t.buyOrderId
                   << " sell#" << t.sellOrderId
                   << "  price=" << t.price
                   << "  qty=" << t.quantity
                   << "  seq=" << t.sequence << "\n";
    }
}

int main() {
    OrderBook book;

    std::cout << "-- Resting sell order --\n";
    printTrades(book.addOrder(Order{1, 100, Side::Sell, 50.0, 10, 0}));

    std::cout << "-- Crossing buy order --\n";
    printTrades(book.addOrder(Order{2, 200, Side::Buy, 51.0, 6, 0}));

    std::cout << "Best ask: ";
    if (auto ask = book.bestAsk()) {
        std::cout << *ask << "\n";
    } else {
        std::cout << "none\n";
    }

    std::cout << "Ask depth: " << book.askDepth() << "\n";

    return 0;
}