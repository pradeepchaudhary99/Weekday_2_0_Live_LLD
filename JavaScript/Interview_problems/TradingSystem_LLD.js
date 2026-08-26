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
    1. Thread-safety (moot for Node's single-threaded event loop, but the
       design would carry over to a worker-thread model).
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

const Side = Object.freeze({ BUY: "BUY", SELL: "SELL" });
const OrderType = Object.freeze({ LIMIT: "LIMIT" });
const OrderStatus = Object.freeze({
    OPEN: "OPEN", PARTIALLY_FILLED: "PARTIALLY_FILLED", FILLED: "FILLED", CANCELLED: "CANCELLED",
});

let _orderSequence = 0;
let _tradeSequence = 0;

class Order {
    constructor(id, symbol, side, type, price, quantity) {
        this.id = id;
        this.symbol = symbol;
        this.side = side;
        this.type = type;
        this.price = price;
        this.quantity = quantity;
        this.remainingQuantity = quantity;
        this.status = OrderStatus.OPEN;
        this.timestamp = ++_orderSequence;
    }
}

class Trade {
    constructor(buyOrderId, sellOrderId, symbol, price, quantity) {
        this.id = `T${++_tradeSequence}`;
        this.buyOrderId = buyOrderId;
        this.sellOrderId = sellOrderId;
        this.symbol = symbol;
        this.price = price;
        this.quantity = quantity;
    }
}

class OrderBook {
    constructor(symbol) {
        this.symbol = symbol;
        this.buyOrders = [];
        this.sellOrders = [];
    }

    addOrder(order) {
        if (order.side === Side.BUY) this.buyOrders.push(order);
        else this.sellOrders.push(order);
    }

    bestBuy() {
        return this.buyOrders.reduce((best, o) => {
            if (!best) return o;
            if (o.price > best.price) return o;
            if (o.price === best.price && o.timestamp < best.timestamp) return o;
            return best;
        }, null);
    }

    bestSell() {
        return this.sellOrders.reduce((best, o) => {
            if (!best) return o;
            if (o.price < best.price) return o;
            if (o.price === best.price && o.timestamp < best.timestamp) return o;
            return best;
        }, null);
    }

    removeOrder(order) {
        const list = order.side === Side.BUY ? this.buyOrders : this.sellOrders;
        const idx = list.indexOf(order);
        if (idx === -1) return false;
        list.splice(idx, 1);
        return true;
    }
}

class MatchingStrategy {
    match(_book, _incoming) {
        throw new Error("match must be implemented by subclasses");
    }
}

class PriceTimePriorityMatching extends MatchingStrategy {
    match(book, incoming) {
        const trades = [];

        while (incoming.remainingQuantity > 0) {
            const resting = incoming.side === Side.BUY ? book.bestSell() : book.bestBuy();
            if (!resting) break;
            if (incoming.side === Side.BUY && resting.price > incoming.price) break;
            if (incoming.side === Side.SELL && resting.price < incoming.price) break;

            const tradeQty = Math.min(incoming.remainingQuantity, resting.remainingQuantity);
            const tradePrice = resting.price;
            const buyOrderId = incoming.side === Side.BUY ? incoming.id : resting.id;
            const sellOrderId = incoming.side === Side.BUY ? resting.id : incoming.id;
            trades.push(new Trade(buyOrderId, sellOrderId, book.symbol, tradePrice, tradeQty));

            incoming.remainingQuantity -= tradeQty;
            resting.remainingQuantity -= tradeQty;

            if (resting.remainingQuantity === 0) {
                resting.status = OrderStatus.FILLED;
                book.removeOrder(resting);
            } else {
                resting.status = OrderStatus.PARTIALLY_FILLED;
            }
        }

        if (incoming.remainingQuantity === 0) {
            incoming.status = OrderStatus.FILLED;
        } else {
            incoming.status = incoming.remainingQuantity < incoming.quantity
                ? OrderStatus.PARTIALLY_FILLED
                : OrderStatus.OPEN;
            book.addOrder(incoming);
        }
        return trades;
    }
}

class OrderNotFoundError extends Error {}
class OrderNotCancellableError extends Error {}

class MatchingEngine {
    constructor(matchingStrategy) {
        this._matchingStrategy = matchingStrategy;
        this._books = new Map();
    }

    _bookFor(symbol) {
        if (!this._books.has(symbol)) {
            this._books.set(symbol, new OrderBook(symbol));
        }
        return this._books.get(symbol);
    }

    placeOrder(order) {
        return this._matchingStrategy.match(this._bookFor(order.symbol), order);
    }

    cancelOrder(order) {
        const book = this._books.get(order.symbol);
        if (!book) return false;
        const removed = book.removeOrder(order);
        if (removed) order.status = OrderStatus.CANCELLED;
        return removed;
    }

    bookForSymbol(symbol) {
        return this._books.get(symbol) ?? null;
    }
}

class TradingSystem {
    constructor(matchingEngine) {
        this._matchingEngine = matchingEngine;
        this._ordersById = new Map();
        this._tradeHistory = [];
        this._orderSequence = 0;
    }

    placeOrder(symbol, side, type, price, quantity) {
        const orderId = `O${++this._orderSequence}`;
        const order = new Order(orderId, symbol, side, type, price, quantity);
        this._ordersById.set(orderId, order);
        const trades = this._matchingEngine.placeOrder(order);
        this._tradeHistory.push(...trades);
        return { order, trades };
    }

    cancelOrder(orderId) {
        const order = this._ordersById.get(orderId);
        if (!order) {
            throw new OrderNotFoundError(`Unknown order ${orderId}`);
        }
        if (order.status !== OrderStatus.OPEN && order.status !== OrderStatus.PARTIALLY_FILLED) {
            throw new OrderNotCancellableError(`Order ${orderId} is ${order.status} and cannot be cancelled`);
        }
        this._matchingEngine.cancelOrder(order);
        return order;
    }

    getOrderStatus(orderId) {
        const order = this._ordersById.get(orderId);
        return order ? order.status : null;
    }

    getTradeHistory() {
        return [...this._tradeHistory];
    }

    getRestingBuyOrders(symbol) {
        const book = this._matchingEngine.bookForSymbol(symbol);
        return book ? [...book.buyOrders] : [];
    }

    getRestingSellOrders(symbol) {
        const book = this._matchingEngine.bookForSymbol(symbol);
        return book ? [...book.sellOrders] : [];
    }
}

function describePlacement({ order, trades }) {
    console.log(`  ${order.id}: ${order.side} ${order.quantity} @ ${order.price.toFixed(2)}`);
    if (trades.length === 0) {
        console.log(`    no match, resting in book (status ${order.status})`);
    } else {
        for (const t of trades) {
            console.log(`    matched ${t.quantity} @ ${t.price.toFixed(2)} (trade ${t.id})`);
        }
        const remaining = order.status === OrderStatus.PARTIALLY_FILLED ? ` (remaining ${order.remainingQuantity})` : "";
        console.log(`    ${order.id} status now: ${order.status}${remaining}`);
    }
}

function main() {
    const system = new TradingSystem(new MatchingEngine(new PriceTimePriorityMatching()));

    console.log("Placing orders on AAPL:");

    const s1 = system.placeOrder("AAPL", Side.SELL, OrderType.LIMIT, 101.00, 10);
    describePlacement(s1);

    const s2 = system.placeOrder("AAPL", Side.SELL, OrderType.LIMIT, 100.50, 5);
    describePlacement(s2);

    const s3 = system.placeOrder("AAPL", Side.SELL, OrderType.LIMIT, 100.75, 3);
    describePlacement(s3);

    console.log("\nA sweeping buy order that crosses multiple price levels:");
    const b1 = system.placeOrder("AAPL", Side.BUY, OrderType.LIMIT, 101.00, 12);
    describePlacement(b1);

    console.log("\nA buy order priced too low to match anything:");
    const b2 = system.placeOrder("AAPL", Side.BUY, OrderType.LIMIT, 99.00, 2);
    describePlacement(b2);

    console.log(`\nCancelling ${b2.order.id} while it still rests in the book:`);
    const cancelled = system.cancelOrder(b2.order.id);
    console.log(`  ${cancelled.id} status now: ${cancelled.status}`);

    console.log("\nFinal order book for AAPL:");
    console.log("  BUY side:");
    for (const o of system.getRestingBuyOrders("AAPL")) {
        console.log(`    ${o.id}: ${o.quantity} @ ${o.price.toFixed(2)} (remaining ${o.remainingQuantity}, ${o.status})`);
    }
    console.log("  SELL side:");
    for (const o of system.getRestingSellOrders("AAPL")) {
        console.log(`    ${o.id}: ${o.quantity} @ ${o.price.toFixed(2)} (remaining ${o.remainingQuantity}, ${o.status})`);
    }

    console.log("\nTrade history:");
    for (const t of system.getTradeHistory()) {
        console.log(`    ${t.id}: ${t.buyOrderId} bought from ${t.sellOrderId}, ${t.quantity} @ ${t.price.toFixed(2)}`);
    }
}

main();
