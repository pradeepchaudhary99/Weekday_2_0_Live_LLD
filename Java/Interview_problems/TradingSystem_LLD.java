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

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.atomic.AtomicLong;

enum Side {
    BUY,
    SELL
}

enum OrderType {
    LIMIT
}

enum OrderStatus {
    OPEN,
    PARTIALLY_FILLED,
    FILLED,
    CANCELLED
}

class Order {
    private static final AtomicLong SEQUENCE = new AtomicLong(0);

    final String id;
    final String symbol;
    final Side side;
    final OrderType type;
    final double price;
    final int quantity;
    int remainingQuantity;
    OrderStatus status = OrderStatus.OPEN;
    final long timestamp;

    Order(String id, String symbol, Side side, OrderType type, double price, int quantity) {
        this.id = id;
        this.symbol = symbol;
        this.side = side;
        this.type = type;
        this.price = price;
        this.quantity = quantity;
        this.remainingQuantity = quantity;
        this.timestamp = SEQUENCE.incrementAndGet();
    }
}

class Trade {
    private static final AtomicLong SEQUENCE = new AtomicLong(0);

    final String id;
    final String buyOrderId;
    final String sellOrderId;
    final String symbol;
    final double price;
    final int quantity;

    Trade(String buyOrderId, String sellOrderId, String symbol, double price, int quantity) {
        this.id = "T" + SEQUENCE.incrementAndGet();
        this.buyOrderId = buyOrderId;
        this.sellOrderId = sellOrderId;
        this.symbol = symbol;
        this.price = price;
        this.quantity = quantity;
    }
}

class OrderBook {
    final String symbol;
    final List<Order> buyOrders = new ArrayList<>();
    final List<Order> sellOrders = new ArrayList<>();

    OrderBook(String symbol) {
        this.symbol = symbol;
    }

    void addOrder(Order order) {
        if (order.side == Side.BUY) buyOrders.add(order);
        else sellOrders.add(order);
    }

    Order bestBuy() {
        Order best = null;
        for (Order o : buyOrders) {
            if (best == null || o.price > best.price || (o.price == best.price && o.timestamp < best.timestamp)) {
                best = o;
            }
        }
        return best;
    }

    Order bestSell() {
        Order best = null;
        for (Order o : sellOrders) {
            if (best == null || o.price < best.price || (o.price == best.price && o.timestamp < best.timestamp)) {
                best = o;
            }
        }
        return best;
    }

    boolean removeOrder(Order order) {
        if (order.side == Side.BUY) return buyOrders.remove(order);
        return sellOrders.remove(order);
    }
}

interface MatchingStrategy {
    List<Trade> match(OrderBook book, Order incoming);
}

class PriceTimePriorityMatching implements MatchingStrategy {
    @Override
    public List<Trade> match(OrderBook book, Order incoming) {
        List<Trade> trades = new ArrayList<>();

        while (incoming.remainingQuantity > 0) {
            Order resting = incoming.side == Side.BUY ? book.bestSell() : book.bestBuy();
            if (resting == null) break;
            if (incoming.side == Side.BUY && resting.price > incoming.price) break;
            if (incoming.side == Side.SELL && resting.price < incoming.price) break;

            int tradeQty = Math.min(incoming.remainingQuantity, resting.remainingQuantity);
            double tradePrice = resting.price;
            String buyOrderId = incoming.side == Side.BUY ? incoming.id : resting.id;
            String sellOrderId = incoming.side == Side.BUY ? resting.id : incoming.id;
            trades.add(new Trade(buyOrderId, sellOrderId, book.symbol, tradePrice, tradeQty));

            incoming.remainingQuantity -= tradeQty;
            resting.remainingQuantity -= tradeQty;

            if (resting.remainingQuantity == 0) {
                resting.status = OrderStatus.FILLED;
                book.removeOrder(resting);
            } else {
                resting.status = OrderStatus.PARTIALLY_FILLED;
            }
        }

        if (incoming.remainingQuantity == 0) {
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

class OrderNotFoundException extends RuntimeException {
    OrderNotFoundException(String message) {
        super(message);
    }
}

class OrderNotCancellableException extends RuntimeException {
    OrderNotCancellableException(String message) {
        super(message);
    }
}

class MatchingEngine {
    private final Map<String, OrderBook> books = new LinkedHashMap<>();
    private final MatchingStrategy matchingStrategy;

    MatchingEngine(MatchingStrategy matchingStrategy) {
        this.matchingStrategy = matchingStrategy;
    }

    private OrderBook bookFor(String symbol) {
        return books.computeIfAbsent(symbol, OrderBook::new);
    }

    List<Trade> placeOrder(Order order) {
        return matchingStrategy.match(bookFor(order.symbol), order);
    }

    boolean cancelOrder(Order order) {
        OrderBook book = books.get(order.symbol);
        if (book == null) return false;
        boolean removed = book.removeOrder(order);
        if (removed) order.status = OrderStatus.CANCELLED;
        return removed;
    }

    OrderBook bookForSymbol(String symbol) {
        return books.get(symbol);
    }
}

class OrderPlacementResult {
    final Order order;
    final List<Trade> trades;

    OrderPlacementResult(Order order, List<Trade> trades) {
        this.order = order;
        this.trades = trades;
    }
}

class TradingSystem {
    private final MatchingEngine matchingEngine;
    private final Map<String, Order> ordersById = new LinkedHashMap<>();
    private final List<Trade> tradeHistory = new ArrayList<>();
    private final AtomicLong orderSequence = new AtomicLong(0);
    private final Object lock = new Object();

    TradingSystem(MatchingEngine matchingEngine) {
        this.matchingEngine = matchingEngine;
    }

    OrderPlacementResult placeOrder(String symbol, Side side, OrderType type, double price, int quantity) {
        synchronized (lock) {
            String orderId = "O" + orderSequence.incrementAndGet();
            Order order = new Order(orderId, symbol, side, type, price, quantity);
            ordersById.put(orderId, order);
            List<Trade> trades = matchingEngine.placeOrder(order);
            tradeHistory.addAll(trades);
            return new OrderPlacementResult(order, trades);
        }
    }

    Order cancelOrder(String orderId) {
        synchronized (lock) {
            Order order = ordersById.get(orderId);
            if (order == null) {
                throw new OrderNotFoundException("Unknown order " + orderId);
            }
            if (order.status != OrderStatus.OPEN && order.status != OrderStatus.PARTIALLY_FILLED) {
                throw new OrderNotCancellableException(
                        "Order " + orderId + " is " + order.status + " and cannot be cancelled");
            }
            matchingEngine.cancelOrder(order);
            return order;
        }
    }

    Optional<OrderStatus> getOrderStatus(String orderId) {
        Order order = ordersById.get(orderId);
        return order == null ? Optional.empty() : Optional.of(order.status);
    }

    List<Trade> getTradeHistory() {
        synchronized (lock) {
            return new ArrayList<>(tradeHistory);
        }
    }

    List<Order> getRestingBuyOrders(String symbol) {
        OrderBook book = matchingEngine.bookForSymbol(symbol);
        return book == null ? List.of() : new ArrayList<>(book.buyOrders);
    }

    List<Order> getRestingSellOrders(String symbol) {
        OrderBook book = matchingEngine.bookForSymbol(symbol);
        return book == null ? List.of() : new ArrayList<>(book.sellOrders);
    }
}

public class TradingSystem_LLD {
    public static void main(String[] args) {
        TradingSystem system = new TradingSystem(new MatchingEngine(new PriceTimePriorityMatching()));

        System.out.println("Placing orders on AAPL:");

        OrderPlacementResult s1 = system.placeOrder("AAPL", Side.SELL, OrderType.LIMIT, 101.00, 10);
        describePlacement(s1);

        OrderPlacementResult s2 = system.placeOrder("AAPL", Side.SELL, OrderType.LIMIT, 100.50, 5);
        describePlacement(s2);

        OrderPlacementResult s3 = system.placeOrder("AAPL", Side.SELL, OrderType.LIMIT, 100.75, 3);
        describePlacement(s3);

        System.out.println("\nA sweeping buy order that crosses multiple price levels:");
        OrderPlacementResult b1 = system.placeOrder("AAPL", Side.BUY, OrderType.LIMIT, 101.00, 12);
        describePlacement(b1);

        System.out.println("\nA buy order priced too low to match anything:");
        OrderPlacementResult b2 = system.placeOrder("AAPL", Side.BUY, OrderType.LIMIT, 99.00, 2);
        describePlacement(b2);

        System.out.println("\nCancelling " + b2.order.id + " while it still rests in the book:");
        Order cancelled = system.cancelOrder(b2.order.id);
        System.out.println("  " + cancelled.id + " status now: " + cancelled.status);

        System.out.println("\nFinal order book for AAPL:");
        System.out.println("  BUY side:");
        for (Order o : system.getRestingBuyOrders("AAPL")) {
            System.out.printf("    %s: %d @ %.2f (remaining %d, %s)%n", o.id, o.quantity, o.price, o.remainingQuantity, o.status);
        }
        System.out.println("  SELL side:");
        for (Order o : system.getRestingSellOrders("AAPL")) {
            System.out.printf("    %s: %d @ %.2f (remaining %d, %s)%n", o.id, o.quantity, o.price, o.remainingQuantity, o.status);
        }

        System.out.println("\nTrade history:");
        for (Trade t : system.getTradeHistory()) {
            System.out.printf("    %s: %s bought from %s, %d @ %.2f%n", t.id, t.buyOrderId, t.sellOrderId, t.quantity, t.price);
        }
    }

    private static void describePlacement(OrderPlacementResult result) {
        Order order = result.order;
        System.out.printf("  %s: %s %d @ %.2f%n", order.id, order.side, order.quantity, order.price);
        if (result.trades.isEmpty()) {
            System.out.println("    no match, resting in book (status " + order.status + ")");
        } else {
            for (Trade t : result.trades) {
                System.out.printf("    matched %d @ %.2f (trade %s)%n", t.quantity, t.price, t.id);
            }
            System.out.println("    " + order.id + " status now: " + order.status
                    + (order.status == OrderStatus.PARTIALLY_FILLED ? " (remaining " + order.remainingQuantity + ")" : ""));
        }
    }
}
