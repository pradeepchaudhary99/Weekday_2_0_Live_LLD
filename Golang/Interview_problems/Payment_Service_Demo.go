/*
================================================================================
LLD: Payment Service
================================================================================

Functional Requirements:
    1. User should be able to create a payment.
    2. Support multiple payment methods:
       - Credit Card
       - Debit Card
       - UPI
       - Net Banking
       - Wallet
    3. Prevent duplicate payments using an idempotency key.
    4. Retry failed payments.
    5. Payment Gateway should be pluggable.

Non-Functional Requirements:
    - Service should avoid duplicate payments.
    - Security (basic input validation before a request ever reaches a
      gateway).
    - Extensible.
    - Thread safe.
    - Retry support.

Design:
    PaymentMethod (Strategy) validates a request is well-formed for its
    channel (card number, UPI id, bank code, ...) before any money moves.

    PaymentGateway (Strategy) is the pluggable boundary to an external
    processor (RazorPay, Stripe, ...). RetryLogic wraps a PaymentGateway
    with the same interface and re-attempts a failed call up to N times --
    this is what the original stub only sketched: it called the inner
    gateway, checked for failure, and then did nothing, so a failed payment
    just vanished with no retry and no response.

    IdempotencyManager is a key -> PaymentResponse cache. A repeat request
    with a key already seen returns the cached result instead of charging
    the user twice -- this is what the original stub got wrong: it checked
    "not a duplicate" before submitting to the worker pool, but never
    checked the "is a duplicate" branch it also declared, and never records
    anything into the manager, so every repeat request would have
    reprocessed the payment.

    PaymentService wires a pluggable gateway, per-type payment methods, and
    the idempotency cache together. It hands gateway calls off to a
    goroutine over a channel (as the original stub's thread-pool intended)
    but -- unlike the original, which fired the task and threw the result
    away -- waits on the channel so CreatePayment can actually return the
    outcome to the caller.

Core Entities:
    PaymentRequest / PaymentResponse / PaymentType / PaymentStatus
    PaymentMethod (Card / UPI / NetBanking)
    PaymentGateway (RazorPay / Stripe) / RetryLogic
    IdempotencyManager
    PaymentService
================================================================================
*/

package main

import (
	"fmt"
	"strings"
	"sync"
)

type PaymentType int

const (
	Card PaymentType = iota
	UPI
	NetBanking
	Wallet
)

func (t PaymentType) String() string {
	switch t {
	case Card:
		return "CARD"
	case UPI:
		return "UPI"
	case NetBanking:
		return "NET_BANKING"
	case Wallet:
		return "WALLET"
	}
	return "UNKNOWN"
}

type PaymentStatus int

const (
	Success PaymentStatus = iota
	Failed
	Duplicate
)

func (s PaymentStatus) String() string {
	switch s {
	case Success:
		return "SUCCESS"
	case Failed:
		return "FAILED"
	case Duplicate:
		return "DUPLICATE"
	}
	return "UNKNOWN"
}

type PaymentRequest struct {
	IdempotencyKey string
	UserID         string
	Amount         float64
	Type           PaymentType
	Details        map[string]string
}

type PaymentResponse struct {
	PaymentID *string
	Status    PaymentStatus
	Message   string
}

func (r PaymentResponse) String() string {
	id := "null"
	if r.PaymentID != nil {
		id = *r.PaymentID
	}
	return fmt.Sprintf("PaymentResponse(payment_id=%s, status=%s, message=%s)", id, r.Status, r.Message)
}

type PaymentMethod interface {
	Validate(request PaymentRequest) bool
}

type CardPaymentMethod struct{}

func (CardPaymentMethod) Validate(request PaymentRequest) bool {
	cardNumber, ok := request.Details["cardNumber"]
	return ok && len(cardNumber) == 16
}

type UpiPaymentMethod struct{}

func (UpiPaymentMethod) Validate(request PaymentRequest) bool {
	upiID, ok := request.Details["upiId"]
	return ok && strings.Contains(upiID, "@")
}

type NetBankingPaymentMethod struct{}

func (NetBankingPaymentMethod) Validate(request PaymentRequest) bool {
	bankCode, ok := request.Details["bankCode"]
	return ok && bankCode != ""
}

type PaymentGateway interface {
	Process(request PaymentRequest) PaymentResponse
}

// RazorPayGateway simulates a gateway that fails the first attempt for a
// given idempotency key and succeeds on every attempt after that -- enough
// to exercise RetryLogic deterministically, without relying on real
// network flakiness.
type RazorPayGateway struct {
	mu       sync.Mutex
	attempts map[string]int
}

func NewRazorPayGateway() *RazorPayGateway {
	return &RazorPayGateway{attempts: make(map[string]int)}
}

func (g *RazorPayGateway) Process(request PaymentRequest) PaymentResponse {
	g.mu.Lock()
	g.attempts[request.IdempotencyKey]++
	attempt := g.attempts[request.IdempotencyKey]
	g.mu.Unlock()

	if attempt == 1 {
		return PaymentResponse{nil, Failed, "RazorPay: transient gateway error (attempt 1)"}
	}
	paymentID := "razorpay-" + request.IdempotencyKey
	return PaymentResponse{&paymentID, Success, fmt.Sprintf("RazorPay: payment captured (attempt %d)", attempt)}
}

// StripeGateway is a second, always-succeeding gateway -- demonstrates that
// PaymentService only depends on the PaymentGateway interface, so the
// processor behind it is pluggable.
type StripeGateway struct{}

func (StripeGateway) Process(request PaymentRequest) PaymentResponse {
	paymentID := "stripe-" + request.IdempotencyKey
	return PaymentResponse{&paymentID, Success, "Stripe: payment captured"}
}

type RetryLogic struct {
	gateway     PaymentGateway
	maxAttempts int
}

func NewRetryLogic(gateway PaymentGateway, maxAttempts int) *RetryLogic {
	return &RetryLogic{gateway: gateway, maxAttempts: maxAttempts}
}

func (r *RetryLogic) Process(request PaymentRequest) PaymentResponse {
	var lastResponse PaymentResponse
	for attempt := 1; attempt <= r.maxAttempts; attempt++ {
		lastResponse = r.gateway.Process(request)
		if lastResponse.Status == Success {
			return lastResponse
		}
	}
	return lastResponse
}

// IdempotencyManager maps key -> the response that was returned the first
// time that key was seen.
type IdempotencyManager struct {
	mu    sync.Mutex
	cache map[string]PaymentResponse
}

func NewIdempotencyManager() *IdempotencyManager {
	return &IdempotencyManager{cache: make(map[string]PaymentResponse)}
}

func (m *IdempotencyManager) GetCached(idempotencyKey string) (PaymentResponse, bool) {
	m.mu.Lock()
	defer m.mu.Unlock()
	response, ok := m.cache[idempotencyKey]
	return response, ok
}

func (m *IdempotencyManager) Record(idempotencyKey string, response PaymentResponse) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.cache[idempotencyKey] = response
}

type PaymentService struct {
	methods            map[PaymentType]PaymentMethod
	gateway            PaymentGateway
	idempotencyManager *IdempotencyManager
}

func NewPaymentService(gateway PaymentGateway) *PaymentService {
	return &PaymentService{
		methods: map[PaymentType]PaymentMethod{
			Card:       CardPaymentMethod{},
			UPI:        UpiPaymentMethod{},
			NetBanking: NetBankingPaymentMethod{},
		},
		gateway:            gateway,
		idempotencyManager: NewIdempotencyManager(),
	}
}

func (s *PaymentService) CreatePayment(request PaymentRequest) PaymentResponse {
	if cached, ok := s.idempotencyManager.GetCached(request.IdempotencyKey); ok {
		return PaymentResponse{cached.PaymentID, Duplicate,
			fmt.Sprintf("Duplicate request for idempotency key %s; returning cached result", request.IdempotencyKey)}
	}

	method, ok := s.methods[request.Type]
	if !ok || !method.Validate(request) {
		return PaymentResponse{nil, Failed, "Validation failed for " + request.Type.String()}
	}

	// Hand the gateway call off to a goroutine over a channel (mirroring a
	// worker-pool submission) and block for the result.
	resultCh := make(chan PaymentResponse, 1)
	go func() {
		resultCh <- s.gateway.Process(request)
	}()
	response := <-resultCh

	s.idempotencyManager.Record(request.IdempotencyKey, response)
	return response
}

func main() {
	service := NewPaymentService(NewRetryLogic(NewRazorPayGateway(), 3))

	fmt.Println("1) Valid card payment (gateway fails attempt #1, RetryLogic recovers it):")
	r1 := service.CreatePayment(PaymentRequest{"key-1", "user-1", 250.00, Card, map[string]string{"cardNumber": "4111111111111111"}})
	fmt.Println("  ", r1)

	fmt.Println("\n2) Same idempotency key resubmitted (must not charge again):")
	r2 := service.CreatePayment(PaymentRequest{"key-1", "user-1", 250.00, Card, map[string]string{"cardNumber": "4111111111111111"}})
	fmt.Println("  ", r2)

	fmt.Println("\n3) Invalid UPI id (missing '@'):")
	r3 := service.CreatePayment(PaymentRequest{"key-2", "user-2", 75.00, UPI, map[string]string{"upiId": "not-a-valid-upi-id"}})
	fmt.Println("  ", r3)

	fmt.Println("\n4) Valid UPI payment on a new idempotency key (also recovers via retry):")
	r4 := service.CreatePayment(PaymentRequest{"key-3", "user-2", 75.00, UPI, map[string]string{"upiId": "user2@upi"}})
	fmt.Println("  ", r4)

	fmt.Println("\n5) Swapping in Stripe for a net-banking payment (gateway is pluggable):")
	stripeService := NewPaymentService(NewRetryLogic(StripeGateway{}, 3))
	r5 := stripeService.CreatePayment(PaymentRequest{"key-4", "user-3", 500.00, NetBanking, map[string]string{"bankCode": "HDFC0001"}})
	fmt.Println("  ", r5)
}
