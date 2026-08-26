/*
================================================================================
LLD: Trading System (Stock Exchange Order Matching Engine)
================================================================================

Functional Requirements:
    1. Place a LIMIT order (BUY or SELL) for a symbol with a price and
       quantity.
    2. Match incoming orders against resting orders in the order book using
       price-time priority: best price first, earliest order first at a
       given price.
    3. Support partial fills: an order can match against several resting
       orders, and a resting order can be partially filled and stay in the
       book for future matches.
    4. Cancel a resting (OPEN or PARTIALLY_FILLED) order.
    5. Query an order's current status and the full trade history.

Non-Functional Requirements:
    1. Thread-safety.
    2. Maintainability / extensibility to new order types and matching
       strategies.
    3. Performance: matching should only look at the best resting orders,
       not rescan the whole book for every order.

Design:
    MatchingStrategy (Strategy) implements the matching algorithm;
    PriceTimePriorityMatching walks the opposite side of the book from the
    best price outward (ties broken by earliest timestamp), filling the
    incoming order against one or more resting orders at each resting
    order's own price, until the incoming order is filled or no resting
    order crosses its price.

    OrderBook holds the resting BUY and SELL orders for one symbol and
    exposes best-price lookups, insertion, and removal.

    MatchingEngine owns one OrderBook per symbol and delegates matching to
    the MatchingStrategy; placeOrder() runs the match and rests whatever
    quantity is left, cancelOrder() removes a resting order from its book.

    TradingSystem (facade) wires the engine together with order/trade
    storage: placeOrder() creates the order, runs it through the engine,
    and records any resulting trades; cancelOrder(), getOrderStatus() and
    getTradeHistory() serve the remaining queries.

Core Entities:
    Side, OrderType, OrderStatus (enums)
    Order
    Trade
    OrderBook
    MatchingStrategy / PriceTimePriorityMatching
    MatchingEngine
    TradingSystem
================================================================================
*/

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

enum class Side { BUY, SELL };
enum class OrderType { LIMIT };
enum class OrderStatus { OPEN, PARTIALLY_FILLED, FILLED, CANCELLED };

std::string sideName(Side side) { return side == Side::BUY ? "BUY" : "SELL"; }

std::string orderStatusName(OrderStatus status) {
    switch (status) {
        case OrderStatus::OPEN: return "OPEN";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
    }
    return "UNKNOWN";
}

class Order {
public:
    std::string id;
    std::string symbol;
    Side side;
    OrderType type;
    double price;
    int quantity;
    int remainingQuantity;
    OrderStatus status = OrderStatus::OPEN;
    long timestamp;

    Order(std::string id, std::string symbol, Side side, OrderType type, double price, int quantity)
        : id(std::move(id)), symbol(std::move(symbol)), side(side), type(type), price(price),
          quantity(quantity), remainingQuantity(quantity) {
        static long sequence = 0;
        timestamp = ++sequence;
    }
};

class Trade {
public:
    std::string id;
    std::string buyOrderId;
    std::string sellOrderId;
    std::string symbol;
    double price;
    int quantity;

    Trade(std::string buyOrderId, std::string sellOrderId, std::string symbol, double price, int quantity)
        : buyOrderId(std::move(buyOrderId)), sellOrderId(std::move(sellOrderId)), symbol(std::move(symbol)),
          price(price), quantity(quantity) {
        static long sequence = 0;
        id = "T" + std::to_string(++sequence);
    }
};

class OrderBook {
public:
    std::string symbol;
    std::vector<std::shared_ptr<Order>> buyOrders;
    std::vector<std::shared_ptr<Order>> sellOrders;

    explicit OrderBook(std::string symbol) : symbol(std::move(symbol)) {}

    void addOrder(const std::shared_ptr<Order>& order) {
        if (order->side == Side::BUY) buyOrders.push_back(order);
        else sellOrders.push_back(order);
    }

    std::shared_ptr<Order> bestBuy() const {
        std::shared_ptr<Order> best = nullptr;
        for (const auto& o : buyOrders) {
            if (!best || o->price > best->price || (o->price == best->price && o->timestamp < best->timestamp)) {
                best = o;
            }
        }
        return best;
    }

    std::shared_ptr<Order> bestSell() const {
        std::shared_ptr<Order> best = nullptr;
        for (const auto& o : sellOrders) {
            if (!best || o->price < best->price || (o->price == best->price && o->timestamp < best->timestamp)) {
                best = o;
            }
        }
        return best;
    }

    bool removeOrder(const std::shared_ptr<Order>& order) {
        auto& list = order->side == Side::BUY ? buyOrders : sellOrders;
        auto it = std::find(list.begin(), list.end(), order);
        if (it == list.end()) return false;
        list.erase(it);
        return true;
    }
};

struct MatchingStrategy {
    virtual ~MatchingStrategy() = default;
    virtual std::vector<Trade> match(OrderBook& book, const std::shared_ptr<Order>& incoming) const = 0;
};

class PriceTimePriorityMatching : public MatchingStrategy {
public:
    std::vector<Trade> match(OrderBook& book, const std::shared_ptr<Order>& incoming) const override {
        std::vector<Trade> trades;

        while (incoming->remainingQuantity > 0) {
            std::shared_ptr<Order> resting = incoming->side == Side::BUY ? book.bestSell() : book.bestBuy();
            if (!resting) break;
            if (incoming->side == Side::BUY && resting->price > incoming->price) break;
            if (incoming->side == Side::SELL && resting->price < incoming->price) break;

            int tradeQty = std::min(incoming->remainingQuantity, resting->remainingQuantity);
            double tradePrice = resting->price;
            std::string buyOrderId = incoming->side == Side::BUY ? incoming->id : resting->id;
            std::string sellOrderId = incoming->side == Side::BUY ? resting->id : incoming->id;
            trades.emplace_back(buyOrderId, sellOrderId, book.symbol, tradePrice, tradeQty);

            incoming->remainingQuantity -= tradeQty;
            resting->remainingQuantity -= tradeQty;

            if (resting->remainingQuantity == 0) {
                resting->status = OrderStatus::FILLED;
                book.removeOrder(resting);
            } else {
                resting->status = OrderStatus::PARTIALLY_FILLED;
            }
        }

        if (incoming->remainingQuantity == 0) {
            incoming->status = OrderStatus::FILLED;
        } else {
            incoming->status = incoming->remainingQuantity < incoming->quantity
                    ? OrderStatus::PARTIALLY_FILLED
                    : OrderStatus::OPEN;
            book.addOrder(incoming);
        }
        return trades;
    }
};

class OrderNotFoundError : public std::runtime_error {
public:
    explicit OrderNotFoundError(const std::string& message) : std::runtime_error(message) {}
};

class OrderNotCancellableError : public std::runtime_error {
public:
    explicit OrderNotCancellableError(const std::string& message) : std::runtime_error(message) {}
};

class MatchingEngine {
public:
    explicit MatchingEngine(std::shared_ptr<MatchingStrategy> matchingStrategy)
        : matchingStrategy_(std::move(matchingStrategy)) {}

    std::vector<Trade> placeOrder(const std::shared_ptr<Order>& order) {
        return matchingStrategy_->match(bookFor(order->symbol), order);
    }

    bool cancelOrder(const std::shared_ptr<Order>& order) {
        auto it = books_.find(order->symbol);
        if (it == books_.end()) return false;
        bool removed = it->second.removeOrder(order);
        if (removed) order->status = OrderStatus::CANCELLED;
        return removed;
    }

    OrderBook* bookForSymbol(const std::string& symbol) {
        auto it = books_.find(symbol);
        return it == books_.end() ? nullptr : &it->second;
    }

private:
    OrderBook& bookFor(const std::string& symbol) {
        auto it = books_.find(symbol);
        if (it == books_.end()) {
            it = books_.emplace(symbol, OrderBook(symbol)).first;
        }
        return it->second;
    }

    std::shared_ptr<MatchingStrategy> matchingStrategy_;
    std::map<std::string, OrderBook> books_;
};

struct OrderPlacementResult {
    std::shared_ptr<Order> order;
    std::vector<Trade> trades;
};

class TradingSystem {
public:
    explicit TradingSystem(std::shared_ptr<MatchingEngine> matchingEngine)
        : matchingEngine_(std::move(matchingEngine)) {}

    OrderPlacementResult placeOrder(const std::string& symbol, Side side, OrderType type, double price, int quantity) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string orderId = "O" + std::to_string(++orderSequence_);
        auto order = std::make_shared<Order>(orderId, symbol, side, type, price, quantity);
        ordersById_[orderId] = order;
        std::vector<Trade> trades = matchingEngine_->placeOrder(order);
        tradeHistory_.insert(tradeHistory_.end(), trades.begin(), trades.end());
        return OrderPlacementResult{order, trades};
    }

    std::shared_ptr<Order> cancelOrder(const std::string& orderId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = ordersById_.find(orderId);
        if (it == ordersById_.end()) {
            throw OrderNotFoundError("Unknown order " + orderId);
        }
        auto order = it->second;
        if (order->status != OrderStatus::OPEN && order->status != OrderStatus::PARTIALLY_FILLED) {
            throw OrderNotCancellableError(
                    "Order " + orderId + " is " + orderStatusName(order->status) + " and cannot be cancelled");
        }
        matchingEngine_->cancelOrder(order);
        return order;
    }

    std::optional<OrderStatus> getOrderStatus(const std::string& orderId) const {
        auto it = ordersById_.find(orderId);
        if (it == ordersById_.end()) return std::nullopt;
        return it->second->status;
    }

    std::vector<Trade> getTradeHistory() const {
        return tradeHistory_;
    }

    std::vector<std::shared_ptr<Order>> getRestingBuyOrders(const std::string& symbol) {
        OrderBook* book = matchingEngine_->bookForSymbol(symbol);
        return book ? book->buyOrders : std::vector<std::shared_ptr<Order>>{};
    }

    std::vector<std::shared_ptr<Order>> getRestingSellOrders(const std::string& symbol) {
        OrderBook* book = matchingEngine_->bookForSymbol(symbol);
        return book ? book->sellOrders : std::vector<std::shared_ptr<Order>>{};
    }

private:
    std::shared_ptr<MatchingEngine> matchingEngine_;
    std::map<std::string, std::shared_ptr<Order>> ordersById_;
    std::vector<Trade> tradeHistory_;
    long orderSequence_ = 0;
    std::mutex mutex_;
};

void describePlacement(const OrderPlacementResult& result) {
    const auto& order = result.order;
    std::printf("  %s: %s %d @ %.2f\n", order->id.c_str(), sideName(order->side).c_str(), order->quantity, order->price);
    if (result.trades.empty()) {
        std::cout << "    no match, resting in book (status " << orderStatusName(order->status) << ")\n";
    } else {
        for (const auto& t : result.trades) {
            std::printf("    matched %d @ %.2f (trade %s)\n", t.quantity, t.price, t.id.c_str());
        }
        std::cout << "    " << order->id << " status now: " << orderStatusName(order->status);
        if (order->status == OrderStatus::PARTIALLY_FILLED) {
            std::cout << " (remaining " << order->remainingQuantity << ")";
        }
        std::cout << "\n";
    }
}

int main() {
    auto system = std::make_shared<TradingSystem>(
            std::make_shared<MatchingEngine>(std::make_shared<PriceTimePriorityMatching>()));

    std::cout << "Placing orders on AAPL:\n";

    auto s1 = system->placeOrder("AAPL", Side::SELL, OrderType::LIMIT, 101.00, 10);
    describePlacement(s1);

    auto s2 = system->placeOrder("AAPL", Side::SELL, OrderType::LIMIT, 100.50, 5);
    describePlacement(s2);

    auto s3 = system->placeOrder("AAPL", Side::SELL, OrderType::LIMIT, 100.75, 3);
    describePlacement(s3);

    std::cout << "\nA sweeping buy order that crosses multiple price levels:\n";
    auto b1 = system->placeOrder("AAPL", Side::BUY, OrderType::LIMIT, 101.00, 12);
    describePlacement(b1);

    std::cout << "\nA buy order priced too low to match anything:\n";
    auto b2 = system->placeOrder("AAPL", Side::BUY, OrderType::LIMIT, 99.00, 2);
    describePlacement(b2);

    std::cout << "\nCancelling " << b2.order->id << " while it still rests in the book:\n";
    auto cancelled = system->cancelOrder(b2.order->id);
    std::cout << "  " << cancelled->id << " status now: " << orderStatusName(cancelled->status) << "\n";

    std::cout << "\nFinal order book for AAPL:\n";
    std::cout << "  BUY side:\n";
    for (const auto& o : system->getRestingBuyOrders("AAPL")) {
        std::printf("    %s: %d @ %.2f (remaining %d, %s)\n", o->id.c_str(), o->quantity, o->price,
                    o->remainingQuantity, orderStatusName(o->status).c_str());
    }
    std::cout << "  SELL side:\n";
    for (const auto& o : system->getRestingSellOrders("AAPL")) {
        std::printf("    %s: %d @ %.2f (remaining %d, %s)\n", o->id.c_str(), o->quantity, o->price,
                    o->remainingQuantity, orderStatusName(o->status).c_str());
    }

    std::cout << "\nTrade history:\n";
    for (const auto& t : system->getTradeHistory()) {
        std::printf("    %s: %s bought from %s, %d @ %.2f\n", t.id.c_str(), t.buyOrderId.c_str(),
                    t.sellOrderId.c_str(), t.quantity, t.price);
    }

    return 0;
}
