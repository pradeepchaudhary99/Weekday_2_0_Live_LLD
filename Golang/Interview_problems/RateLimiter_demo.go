/*
================================================================================
LLD: Rate Limiter
================================================================================

Problem: Design a rate limiter that throttles how many requests a client
(identified by UserID) can make in a given time window, so no single client
can overwhelm the system.

Functional Requirements:
    1. Given a client identifier, decide whether the current request is
       ALLOWED or REJECTED.
    2. Support more than one throttling algorithm (token bucket, sliding
       window) behind a common interface, so the algorithm can be swapped
       without touching call sites.
    3. Limits are tracked independently per client -- one noisy user must
       never affect another user's quota.

Non-Functional Requirements:
    1. O(1) (or near O(1)) decision latency -- rate limiting sits on the hot
       path of every request.
    2. Thread-safety: multiple requests for the same/different users can
       arrive concurrently and must not corrupt shared counters.
    3. Extensibility: adding a new algorithm (e.g. leaky bucket, fixed
       window) should mean adding a new type, not editing existing ones.

Design (Strategy pattern):
    RateLimitingStrategy is the interface every algorithm implements. Each
    strategy owns one bucket/window PER USER, created lazily on first use --
    this is what the original stub got wrong: it looked up a bucket for a
    brand-new user and called a method on the resulting nil pointer. Here a
    mutex-guarded lazy-insert both fixes that and keeps creation atomic
    under concurrent access.

    TokenBucket:      allows short bursts up to Capacity, then refills at a
                       steady RefillRate tokens/sec. Good when you want to
                       tolerate bursty-but-generally-light traffic.
    SlidingWindow:     counts requests in the trailing WindowSize
                       milliseconds using a timestamp slice. Smooths out the
                       bursts a token bucket would allow, at the cost of
                       remembering every timestamp in the window.

Core Entities:
    Request                   -- who is calling (UserID) and from where (IP)
    RateLimitingStrategy       -- the algorithm interface
    TokenBucket / TokenBucketStrategy
    SlidingWindow / SlidingWindowRateLimiting
================================================================================
*/

package main

import (
	"fmt"
	"sync"
	"time"
)

type Request struct {
	UserID string
	IP     string
}

type RateLimitingStrategy interface {
	Allowed(request *Request) bool
}

// One bucket per user. Guarded by the strategy's mutex, not thread-safe on
// its own.
type TokenBucket struct {
	Capacity       int
	Tokens         int
	LastRefillTime int64 // unix millis
	RefillRate     int   // tokens added per second
}

func NewTokenBucket(capacity, refillRate int) *TokenBucket {
	return &TokenBucket{
		Capacity:       capacity,
		Tokens:         capacity, // start full: first burst is allowed immediately
		LastRefillTime: time.Now().UnixMilli(),
		RefillRate:     refillRate,
	}
}

func (t *TokenBucket) IsAllowed() bool {
	t.Refill()
	if t.Tokens > 0 {
		t.Tokens--
		return true
	}
	return false
}

func (t *TokenBucket) Refill() {
	currentTime := time.Now().UnixMilli()
	timeDiffSeconds := (currentTime - t.LastRefillTime) / 1000
	if timeDiffSeconds <= 0 {
		return // avoid resetting LastRefillTime before a whole second has passed
	}
	claimedTokens := int(timeDiffSeconds) * t.RefillRate
	if t.Tokens+claimedTokens < t.Capacity {
		t.Tokens += claimedTokens
	} else {
		t.Tokens = t.Capacity
	}
	t.LastRefillTime = currentTime
}

// Akash, Nishaanth
type TokenBucketStrategy struct {
	capacity     int
	refillRate   int
	tokenBuckets map[string]*TokenBucket
	mutex        sync.Mutex
}

func NewTokenBucketStrategy(capacity, refillRate int) *TokenBucketStrategy {
	return &TokenBucketStrategy{
		capacity:     capacity,
		refillRate:   refillRate,
		tokenBuckets: make(map[string]*TokenBucket),
	}
}

func (s *TokenBucketStrategy) Allowed(request *Request) bool {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	// Lazily create a bucket for a first-time user. Holding the mutex for
	// the lookup-or-create guarantees only one bucket is ever created per
	// user under race.
	bucket, ok := s.tokenBuckets[request.UserID]
	if !ok {
		bucket = NewTokenBucket(s.capacity, s.refillRate)
		s.tokenBuckets[request.UserID] = bucket
	}
	return bucket.IsAllowed()
}

// One sliding window per user, storing the timestamp of every accepted
// request in the trailing WindowSize milliseconds.
type SlidingWindow struct {
	Capacity   int
	Window     []int64
	WindowSize int64 // milliseconds
}

func NewSlidingWindow(capacity int, windowSize int64) *SlidingWindow {
	return &SlidingWindow{Capacity: capacity, WindowSize: windowSize}
}

func (w *SlidingWindow) IsAllowed() bool {
	currentTime := time.Now().UnixMilli()
	windowStart := currentTime - w.WindowSize
	for len(w.Window) > 0 && w.Window[0] < windowStart {
		w.Window = w.Window[1:]
	}
	if len(w.Window) < w.Capacity {
		w.Window = append(w.Window, currentTime)
		return true
	}
	return false
}

type SlidingWindowRateLimiting struct {
	capacity   int
	windowSize int64
	windows    map[string]*SlidingWindow
	mutex      sync.Mutex
}

func NewSlidingWindowRateLimiting(capacity int, windowSize int64) *SlidingWindowRateLimiting {
	return &SlidingWindowRateLimiting{
		capacity:   capacity,
		windowSize: windowSize,
		windows:    make(map[string]*SlidingWindow),
	}
}

func (s *SlidingWindowRateLimiting) Allowed(request *Request) bool {
	s.mutex.Lock()
	defer s.mutex.Unlock()
	window, ok := s.windows[request.UserID]
	if !ok {
		window = NewSlidingWindow(s.capacity, s.windowSize)
		s.windows[request.UserID] = window
	}
	return window.IsAllowed()
}

func fireBurst(strategy RateLimitingStrategy, label, userID string, count int) {
	fmt.Printf("%s -- firing %d back-to-back requests for %s:\n", label, count, userID)
	for i := 1; i <= count; i++ {
		ok := strategy.Allowed(&Request{UserID: userID, IP: "10.0.0.1"})
		result := "REJECTED"
		if ok {
			result = "ALLOWED"
		}
		fmt.Printf("  request #%d -> %s\n", i, result)
	}
}

func main() {
	// Capacity 3, refills 1 token/sec: the first 3 requests burst through,
	// the 4th is rejected immediately after.
	tokenBucket := NewTokenBucketStrategy(3, 1)
	fireBurst(tokenBucket, "TokenBucket", "alice", 4)

	fmt.Println("\nWaiting 1.1s for the token bucket to refill one token...")
	time.Sleep(1100 * time.Millisecond)
	afterRefill := tokenBucket.Allowed(&Request{UserID: "alice", IP: "10.0.0.1"})
	fmt.Printf("request after refill -> %s\n", allowedLabel(afterRefill))

	fmt.Println()
	// A second, independent user must not be affected by alice's usage.
	fireBurst(tokenBucket, "TokenBucket", "bob", 2)

	fmt.Println()
	// Capacity 3 requests per 500ms sliding window.
	slidingWindow := NewSlidingWindowRateLimiting(3, 500)
	fireBurst(slidingWindow, "SlidingWindow", "carol", 4)

	fmt.Println("\nWaiting 600ms for carol's window to fully slide past...")
	time.Sleep(600 * time.Millisecond)
	afterSlide := slidingWindow.Allowed(&Request{UserID: "carol", IP: "10.0.0.1"})
	fmt.Printf("request after window slides -> %s\n", allowedLabel(afterSlide))
}

func allowedLabel(ok bool) string {
	if ok {
		return "ALLOWED"
	}
	return "REJECTED"
}
