package main

import (
	"fmt"
	"time"
)

type Request struct {
	clientID  string
	timestamp float64
}

func NewRequest(clientID string) Request {
	return Request{clientID: clientID, timestamp: nowSeconds()}
}

func nowSeconds() float64 {
	return float64(time.Now().UnixNano()) / 1e9
}

type RateLimitingStrategy interface {
	IsAllowed(request Request) bool
}

type TokenBucketStrategy struct {
	capacity         float64
	refillRatePerSec float64
	tokens           float64
	lastRefill       float64
}

func NewTokenBucketStrategy(capacity int, refillRatePerSec float64) *TokenBucketStrategy {
	return &TokenBucketStrategy{
		capacity:         float64(capacity),
		refillRatePerSec: refillRatePerSec,
		tokens:           float64(capacity),
		lastRefill:       nowSeconds(),
	}
}

func (s *TokenBucketStrategy) refill(now float64) {
	elapsed := now - s.lastRefill
	s.tokens = min(s.capacity, s.tokens+elapsed*s.refillRatePerSec)
	s.lastRefill = now
}

func (s *TokenBucketStrategy) IsAllowed(request Request) bool {
	s.refill(request.timestamp)
	if s.tokens >= 1 {
		s.tokens--
		return true
	}
	return false
}

type SlidingWindowStrategy struct {
	windowSizeSec float64
	maxRequests   int
	timestamps    []float64
}

func NewSlidingWindowStrategy(windowSizeSec float64, maxRequests int) *SlidingWindowStrategy {
	return &SlidingWindowStrategy{windowSizeSec: windowSizeSec, maxRequests: maxRequests}
}

func (s *SlidingWindowStrategy) IsAllowed(request Request) bool {
	now := request.timestamp
	for len(s.timestamps) > 0 && s.timestamps[0] <= now-s.windowSizeSec {
		s.timestamps = s.timestamps[1:]
	}
	if len(s.timestamps) < s.maxRequests {
		s.timestamps = append(s.timestamps, now)
		return true
	}
	return false
}

type NotificationService struct {
	strategy RateLimitingStrategy
}

func (n *NotificationService) SetStrategy(strategy RateLimitingStrategy) {
	n.strategy = strategy
}

func (n *NotificationService) Send(request Request) {
	if n.strategy == nil {
		panic("No rate limiting strategy configured")
	}
	if n.strategy.IsAllowed(request) {
		fmt.Printf("Notification sent for client %s\n", request.clientID)
	} else {
		fmt.Printf("Rate limit exceeded for client %s\n", request.clientID)
	}
}

func min(a, b float64) float64 {
	if a < b {
		return a
	}
	return b
}

func main() {
	service := &NotificationService{}

	service.SetStrategy(NewTokenBucketStrategy(3, 1))
	fmt.Println("-- Token Bucket --")
	for i := 0; i < 5; i++ {
		service.Send(NewRequest("user-1"))
	}

	service.SetStrategy(NewSlidingWindowStrategy(60, 2))
	fmt.Println("-- Sliding Window --")
	for i := 0; i < 4; i++ {
		service.Send(NewRequest("user-2"))
	}
}
