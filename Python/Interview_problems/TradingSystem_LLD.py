"""
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
    the MatchingStrategy; place_order() runs the match and rests whatever
    quantity is left, cancel_order() removes a resting order from its book.

    TradingSystem (facade) wires the engine together with order/trade
    storage: place_order() creates the order, runs it through the engine,
    and records any resulting trades; cancel_order(), get_order_status()
    and get_trade_history() serve the remaining queries.

Core Entities:
    Side, OrderType, OrderStatus (enums)
    Order
    Trade
    OrderBook
    MatchingStrategy / PriceTimePriorityMatching
    MatchingEngine
    TradingSystem
================================================================================
"""

import itertools
import threading
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Dict, List, Optional


class Side(Enum):
    BUY = auto()
    SELL = auto()


class OrderType(Enum):
    LIMIT = auto()


class OrderStatus(Enum):
    OPEN = auto()
    PARTIALLY_FILLED = auto()
    FILLED = auto()
    CANCELLED = auto()


_order_sequence = itertools.count(1)
_trade_sequence = itertools.count(1)


@dataclass
class Order:
    id: str
    symbol: str
    side: Side
    type: OrderType
    price: float
    quantity: int
    remaining_quantity: int = field(init=False)
    status: OrderStatus = field(default=OrderStatus.OPEN, init=False)
    timestamp: int = field(init=False)

    def __post_init__(self):
        self.remaining_quantity = self.quantity
        self.timestamp = next(_order_sequence)


@dataclass
class Trade:
    buy_order_id: str
    sell_order_id: str
    symbol: str
    price: float
    quantity: int
    id: str = field(init=False)

    def __post_init__(self):
        self.id = f"T{next(_trade_sequence)}"


class OrderBook:
    def __init__(self, symbol: str):
        self.symbol = symbol
        self.buy_orders: List[Order] = []
        self.sell_orders: List[Order] = []

    def add_order(self, order: Order) -> None:
        if order.side == Side.BUY:
            self.buy_orders.append(order)
        else:
            self.sell_orders.append(order)

    def best_buy(self) -> Optional[Order]:
        if not self.buy_orders:
            return None
        return min(self.buy_orders, key=lambda o: (-o.price, o.timestamp))

    def best_sell(self) -> Optional[Order]:
        if not self.sell_orders:
            return None
        return min(self.sell_orders, key=lambda o: (o.price, o.timestamp))

    def remove_order(self, order: Order) -> bool:
        side_list = self.buy_orders if order.side == Side.BUY else self.sell_orders
        if order in side_list:
            side_list.remove(order)
            return True
        return False


class MatchingStrategy(ABC):
    @abstractmethod
    def match(self, book: OrderBook, incoming: Order) -> List[Trade]:
        raise NotImplementedError


class PriceTimePriorityMatching(MatchingStrategy):
    def match(self, book: OrderBook, incoming: Order) -> List[Trade]:
        trades: List[Trade] = []

        while incoming.remaining_quantity > 0:
            resting = book.best_sell() if incoming.side == Side.BUY else book.best_buy()
            if resting is None:
                break
            if incoming.side == Side.BUY and resting.price > incoming.price:
                break
            if incoming.side == Side.SELL and resting.price < incoming.price:
                break

            trade_qty = min(incoming.remaining_quantity, resting.remaining_quantity)
            trade_price = resting.price
            buy_order_id = incoming.id if incoming.side == Side.BUY else resting.id
            sell_order_id = resting.id if incoming.side == Side.BUY else incoming.id
            trades.append(Trade(buy_order_id, sell_order_id, book.symbol, trade_price, trade_qty))

            incoming.remaining_quantity -= trade_qty
            resting.remaining_quantity -= trade_qty

            if resting.remaining_quantity == 0:
                resting.status = OrderStatus.FILLED
                book.remove_order(resting)
            else:
                resting.status = OrderStatus.PARTIALLY_FILLED

        if incoming.remaining_quantity == 0:
            incoming.status = OrderStatus.FILLED
        else:
            incoming.status = (
                OrderStatus.PARTIALLY_FILLED
                if incoming.remaining_quantity < incoming.quantity
                else OrderStatus.OPEN
            )
            book.add_order(incoming)

        return trades


class OrderNotFoundError(RuntimeError):
    pass


class OrderNotCancellableError(RuntimeError):
    pass


class MatchingEngine:
    def __init__(self, matching_strategy: MatchingStrategy):
        self._matching_strategy = matching_strategy
        self._books: Dict[str, OrderBook] = {}

    def _book_for(self, symbol: str) -> OrderBook:
        if symbol not in self._books:
            self._books[symbol] = OrderBook(symbol)
        return self._books[symbol]

    def place_order(self, order: Order) -> List[Trade]:
        return self._matching_strategy.match(self._book_for(order.symbol), order)

    def cancel_order(self, order: Order) -> bool:
        book = self._books.get(order.symbol)
        if book is None:
            return False
        removed = book.remove_order(order)
        if removed:
            order.status = OrderStatus.CANCELLED
        return removed

    def book_for_symbol(self, symbol: str) -> Optional[OrderBook]:
        return self._books.get(symbol)


@dataclass
class OrderPlacementResult:
    order: Order
    trades: List[Trade]


class TradingSystem:
    def __init__(self, matching_engine: MatchingEngine):
        self._matching_engine = matching_engine
        self._orders_by_id: Dict[str, Order] = {}
        self._trade_history: List[Trade] = []
        self._order_sequence = itertools.count(1)
        self._lock = threading.Lock()

    def place_order(self, symbol: str, side: Side, order_type: OrderType, price: float, quantity: int) -> OrderPlacementResult:
        with self._lock:
            order_id = f"O{next(self._order_sequence)}"
            order = Order(order_id, symbol, side, order_type, price, quantity)
            self._orders_by_id[order_id] = order
            trades = self._matching_engine.place_order(order)
            self._trade_history.extend(trades)
            return OrderPlacementResult(order, trades)

    def cancel_order(self, order_id: str) -> Order:
        with self._lock:
            order = self._orders_by_id.get(order_id)
            if order is None:
                raise OrderNotFoundError(f"Unknown order {order_id}")
            if order.status not in (OrderStatus.OPEN, OrderStatus.PARTIALLY_FILLED):
                raise OrderNotCancellableError(
                    f"Order {order_id} is {order.status.name} and cannot be cancelled")
            self._matching_engine.cancel_order(order)
            return order

    def get_order_status(self, order_id: str) -> Optional[OrderStatus]:
        order = self._orders_by_id.get(order_id)
        return order.status if order else None

    def get_trade_history(self) -> List[Trade]:
        with self._lock:
            return list(self._trade_history)

    def get_resting_buy_orders(self, symbol: str) -> List[Order]:
        book = self._matching_engine.book_for_symbol(symbol)
        return list(book.buy_orders) if book else []

    def get_resting_sell_orders(self, symbol: str) -> List[Order]:
        book = self._matching_engine.book_for_symbol(symbol)
        return list(book.sell_orders) if book else []


def _describe_placement(result: OrderPlacementResult) -> None:
    order = result.order
    print(f"  {order.id}: {order.side.name} {order.quantity} @ {order.price:.2f}")
    if not result.trades:
        print(f"    no match, resting in book (status {order.status.name})")
    else:
        for t in result.trades:
            print(f"    matched {t.quantity} @ {t.price:.2f} (trade {t.id})")
        remaining = f" (remaining {order.remaining_quantity})" if order.status == OrderStatus.PARTIALLY_FILLED else ""
        print(f"    {order.id} status now: {order.status.name}{remaining}")


def main() -> None:
    system = TradingSystem(MatchingEngine(PriceTimePriorityMatching()))

    print("Placing orders on AAPL:")

    s1 = system.place_order("AAPL", Side.SELL, OrderType.LIMIT, 101.00, 10)
    _describe_placement(s1)

    s2 = system.place_order("AAPL", Side.SELL, OrderType.LIMIT, 100.50, 5)
    _describe_placement(s2)

    s3 = system.place_order("AAPL", Side.SELL, OrderType.LIMIT, 100.75, 3)
    _describe_placement(s3)

    print("\nA sweeping buy order that crosses multiple price levels:")
    b1 = system.place_order("AAPL", Side.BUY, OrderType.LIMIT, 101.00, 12)
    _describe_placement(b1)

    print("\nA buy order priced too low to match anything:")
    b2 = system.place_order("AAPL", Side.BUY, OrderType.LIMIT, 99.00, 2)
    _describe_placement(b2)

    print(f"\nCancelling {b2.order.id} while it still rests in the book:")
    cancelled = system.cancel_order(b2.order.id)
    print(f"  {cancelled.id} status now: {cancelled.status.name}")

    print("\nFinal order book for AAPL:")
    print("  BUY side:")
    for o in system.get_resting_buy_orders("AAPL"):
        print(f"    {o.id}: {o.quantity} @ {o.price:.2f} (remaining {o.remaining_quantity}, {o.status.name})")
    print("  SELL side:")
    for o in system.get_resting_sell_orders("AAPL"):
        print(f"    {o.id}: {o.quantity} @ {o.price:.2f} (remaining {o.remaining_quantity}, {o.status.name})")

    print("\nTrade history:")
    for t in system.get_trade_history():
        print(f"    {t.id}: {t.buy_order_id} bought from {t.sell_order_id}, {t.quantity} @ {t.price:.2f}")


if __name__ == "__main__":
    main()
