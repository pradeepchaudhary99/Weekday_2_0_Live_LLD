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
    the MatchingStrategy; PlaceOrder() runs the match and rests whatever
    quantity is left, CancelOrder() removes a resting order from its book.

    TradingSystem (facade) wires the engine together with order/trade
    storage: PlaceOrder() creates the order, runs it through the engine,
    and records any resulting trades; CancelOrder(), GetOrderStatus() and
    GetTradeHistory() serve the remaining queries.

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

package main

import (
	"errors"
	"fmt"
	"sync"
)

type Side int

const (
	Buy Side = iota
	Sell
)

func (s Side) String() string {
	if s == Buy {
		return "BUY"
	}
	return "SELL"
}

type OrderType int

const (
	Limit OrderType = iota
)

func (t OrderType) String() string {
	return "LIMIT"
}

type OrderStatus int

const (
	Open OrderStatus = iota
	PartiallyFilled
	Filled
	Cancelled
)

func (s OrderStatus) String() string {
	switch s {
	case Open:
		return "OPEN"
	case PartiallyFilled:
		return "PARTIALLY_FILLED"
	case Filled:
		return "FILLED"
	case Cancelled:
		return "CANCELLED"
	}
	return "UNKNOWN"
}

type Order struct {
	ID                string
	Symbol            string
	Side              Side
	Type              OrderType
	Price             float64
	Quantity          int
	RemainingQuantity int
	Status            OrderStatus
	Timestamp         int64
}

type Trade struct {
	ID          string
	BuyOrderID  string
	SellOrderID string
	Symbol      string
	Price       float64
	Quantity    int
}

type OrderBook struct {
	Symbol     string
	BuyOrders  []*Order
	SellOrders []*Order
}

func NewOrderBook(symbol string) *OrderBook {
	return &OrderBook{Symbol: symbol}
}

func (b *OrderBook) AddOrder(order *Order) {
	if order.Side == Buy {
		b.BuyOrders = append(b.BuyOrders, order)
	} else {
		b.SellOrders = append(b.SellOrders, order)
	}
}

func (b *OrderBook) BestBuy() *Order {
	var best *Order
	for _, o := range b.BuyOrders {
		if best == nil || o.Price > best.Price || (o.Price == best.Price && o.Timestamp < best.Timestamp) {
			best = o
		}
	}
	return best
}

func (b *OrderBook) BestSell() *Order {
	var best *Order
	for _, o := range b.SellOrders {
		if best == nil || o.Price < best.Price || (o.Price == best.Price && o.Timestamp < best.Timestamp) {
			best = o
		}
	}
	return best
}

func (b *OrderBook) RemoveOrder(order *Order) bool {
	list := &b.BuyOrders
	if order.Side == Sell {
		list = &b.SellOrders
	}
	for i, o := range *list {
		if o == order {
			*list = append((*list)[:i], (*list)[i+1:]...)
			return true
		}
	}
	return false
}

type MatchingStrategy interface {
	Match(book *OrderBook, incoming *Order) []Trade
}

type PriceTimePriorityMatching struct {
	tradeSequence *int64
}

func (m PriceTimePriorityMatching) Match(book *OrderBook, incoming *Order) []Trade {
	var trades []Trade

	for incoming.RemainingQuantity > 0 {
		var resting *Order
		if incoming.Side == Buy {
			resting = book.BestSell()
		} else {
			resting = book.BestBuy()
		}
		if resting == nil {
			break
		}
		if incoming.Side == Buy && resting.Price > incoming.Price {
			break
		}
		if incoming.Side == Sell && resting.Price < incoming.Price {
			break
		}

		tradeQty := incoming.RemainingQuantity
		if resting.RemainingQuantity < tradeQty {
			tradeQty = resting.RemainingQuantity
		}
		tradePrice := resting.Price
		buyOrderID, sellOrderID := incoming.ID, resting.ID
		if incoming.Side == Sell {
			buyOrderID, sellOrderID = resting.ID, incoming.ID
		}
		*m.tradeSequence++
		trades = append(trades, Trade{
			ID: fmt.Sprintf("T%d", *m.tradeSequence), BuyOrderID: buyOrderID, SellOrderID: sellOrderID,
			Symbol: book.Symbol, Price: tradePrice, Quantity: tradeQty,
		})

		incoming.RemainingQuantity -= tradeQty
		resting.RemainingQuantity -= tradeQty

		if resting.RemainingQuantity == 0 {
			resting.Status = Filled
			book.RemoveOrder(resting)
		} else {
			resting.Status = PartiallyFilled
		}
	}

	if incoming.RemainingQuantity == 0 {
		incoming.Status = Filled
	} else {
		if incoming.RemainingQuantity < incoming.Quantity {
			incoming.Status = PartiallyFilled
		} else {
			incoming.Status = Open
		}
		book.AddOrder(incoming)
	}
	return trades
}

var (
	ErrOrderNotFound       = errors.New("unknown order")
	ErrOrderNotCancellable = errors.New("order cannot be cancelled")
)

type MatchingEngine struct {
	matchingStrategy MatchingStrategy
	books            map[string]*OrderBook
}

func NewMatchingEngine(matchingStrategy MatchingStrategy) *MatchingEngine {
	return &MatchingEngine{matchingStrategy: matchingStrategy, books: make(map[string]*OrderBook)}
}

func (e *MatchingEngine) bookFor(symbol string) *OrderBook {
	book, ok := e.books[symbol]
	if !ok {
		book = NewOrderBook(symbol)
		e.books[symbol] = book
	}
	return book
}

func (e *MatchingEngine) PlaceOrder(order *Order) []Trade {
	return e.matchingStrategy.Match(e.bookFor(order.Symbol), order)
}

func (e *MatchingEngine) CancelOrder(order *Order) bool {
	book, ok := e.books[order.Symbol]
	if !ok {
		return false
	}
	removed := book.RemoveOrder(order)
	if removed {
		order.Status = Cancelled
	}
	return removed
}

func (e *MatchingEngine) BookForSymbol(symbol string) *OrderBook {
	return e.books[symbol]
}

type OrderPlacementResult struct {
	Order  *Order
	Trades []Trade
}

type TradingSystem struct {
	mu             sync.Mutex
	matchingEngine *MatchingEngine
	ordersByID     map[string]*Order
	tradeHistory   []Trade
	orderSequence  int64
}

func NewTradingSystem(matchingEngine *MatchingEngine) *TradingSystem {
	return &TradingSystem{matchingEngine: matchingEngine, ordersByID: make(map[string]*Order)}
}

func (s *TradingSystem) PlaceOrder(symbol string, side Side, orderType OrderType, price float64, quantity int) OrderPlacementResult {
	s.mu.Lock()
	defer s.mu.Unlock()
	s.orderSequence++
	order := &Order{
		ID: fmt.Sprintf("O%d", s.orderSequence), Symbol: symbol, Side: side, Type: orderType,
		Price: price, Quantity: quantity, RemainingQuantity: quantity, Status: Open, Timestamp: s.orderSequence,
	}
	s.ordersByID[order.ID] = order
	trades := s.matchingEngine.PlaceOrder(order)
	s.tradeHistory = append(s.tradeHistory, trades...)
	return OrderPlacementResult{Order: order, Trades: trades}
}

func (s *TradingSystem) CancelOrder(orderID string) (*Order, error) {
	s.mu.Lock()
	defer s.mu.Unlock()
	order, ok := s.ordersByID[orderID]
	if !ok {
		return nil, fmt.Errorf("%w: %s", ErrOrderNotFound, orderID)
	}
	if order.Status != Open && order.Status != PartiallyFilled {
		return nil, fmt.Errorf("%w: order %s is %s", ErrOrderNotCancellable, orderID, order.Status)
	}
	s.matchingEngine.CancelOrder(order)
	return order, nil
}

func (s *TradingSystem) GetOrderStatus(orderID string) (OrderStatus, bool) {
	order, ok := s.ordersByID[orderID]
	if !ok {
		return 0, false
	}
	return order.Status, true
}

func (s *TradingSystem) GetTradeHistory() []Trade {
	s.mu.Lock()
	defer s.mu.Unlock()
	history := make([]Trade, len(s.tradeHistory))
	copy(history, s.tradeHistory)
	return history
}

func (s *TradingSystem) GetRestingBuyOrders(symbol string) []*Order {
	book := s.matchingEngine.BookForSymbol(symbol)
	if book == nil {
		return nil
	}
	return book.BuyOrders
}

func (s *TradingSystem) GetRestingSellOrders(symbol string) []*Order {
	book := s.matchingEngine.BookForSymbol(symbol)
	if book == nil {
		return nil
	}
	return book.SellOrders
}

func describePlacement(result OrderPlacementResult) {
	order := result.Order
	fmt.Printf("  %s: %s %d @ %.2f\n", order.ID, order.Side, order.Quantity, order.Price)
	if len(result.Trades) == 0 {
		fmt.Printf("    no match, resting in book (status %s)\n", order.Status)
		return
	}
	for _, t := range result.Trades {
		fmt.Printf("    matched %d @ %.2f (trade %s)\n", t.Quantity, t.Price, t.ID)
	}
	remaining := ""
	if order.Status == PartiallyFilled {
		remaining = fmt.Sprintf(" (remaining %d)", order.RemainingQuantity)
	}
	fmt.Printf("    %s status now: %s%s\n", order.ID, order.Status, remaining)
}

func main() {
	var tradeSequence int64
	system := NewTradingSystem(NewMatchingEngine(PriceTimePriorityMatching{tradeSequence: &tradeSequence}))

	fmt.Println("Placing orders on AAPL:")

	s1 := system.PlaceOrder("AAPL", Sell, Limit, 101.00, 10)
	describePlacement(s1)

	s2 := system.PlaceOrder("AAPL", Sell, Limit, 100.50, 5)
	describePlacement(s2)

	s3 := system.PlaceOrder("AAPL", Sell, Limit, 100.75, 3)
	describePlacement(s3)

	fmt.Println("\nA sweeping buy order that crosses multiple price levels:")
	b1 := system.PlaceOrder("AAPL", Buy, Limit, 101.00, 12)
	describePlacement(b1)

	fmt.Println("\nA buy order priced too low to match anything:")
	b2 := system.PlaceOrder("AAPL", Buy, Limit, 99.00, 2)
	describePlacement(b2)

	fmt.Printf("\nCancelling %s while it still rests in the book:\n", b2.Order.ID)
	cancelled, _ := system.CancelOrder(b2.Order.ID)
	fmt.Printf("  %s status now: %s\n", cancelled.ID, cancelled.Status)

	fmt.Println("\nFinal order book for AAPL:")
	fmt.Println("  BUY side:")
	for _, o := range system.GetRestingBuyOrders("AAPL") {
		fmt.Printf("    %s: %d @ %.2f (remaining %d, %s)\n", o.ID, o.Quantity, o.Price, o.RemainingQuantity, o.Status)
	}
	fmt.Println("  SELL side:")
	for _, o := range system.GetRestingSellOrders("AAPL") {
		fmt.Printf("    %s: %d @ %.2f (remaining %d, %s)\n", o.ID, o.Quantity, o.Price, o.RemainingQuantity, o.Status)
	}

	fmt.Println("\nTrade history:")
	for _, t := range system.GetTradeHistory() {
		fmt.Printf("    %s: %s bought from %s, %d @ %.2f\n", t.ID, t.BuyOrderID, t.SellOrderID, t.Quantity, t.Price)
	}
}
