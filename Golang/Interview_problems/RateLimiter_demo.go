package main

import (
	"time"
)

type RateLimitingStrategy interface {
	Allowed(request *Request) bool
}

type TokenBucket struct {
	Capacity       int
	Tokens         int
	LastRefillTime int64
	RefillRate     int
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
	timeDiff := (currentTime - t.LastRefillTime) / 1000
	claimedTokens := int(timeDiff) * t.RefillRate
	if t.Tokens+claimedTokens < t.Capacity {
		t.Tokens = t.Tokens + claimedTokens
	} else {
		t.Tokens = t.Capacity
	}
	t.LastRefillTime = currentTime
}

type Request struct {
	UserID string
	IP     string
}

// Akash, Nishaanth
type TokenBucketStrategy struct {
	TokenBuckets map[string]*TokenBucket
}

func NewTokenBucketStrategy() *TokenBucketStrategy {
	return &TokenBucketStrategy{TokenBuckets: make(map[string]*TokenBucket)}
}

func (s *TokenBucketStrategy) Allowed(request *Request) bool {
	bucket := s.TokenBuckets[request.UserID]

	if bucket.IsAllowed() {
		// its allowed do the work
		return true
	}
	// error codes
	return false
}

type SlidingWindow struct {
	Capacity   int
	Window     []int64
	WindowSize int64
}

func (w *SlidingWindow) IsAllowed() bool {
	currentTime := time.Now().UnixMilli()
	startWindow := currentTime - w.WindowSize
	for len(w.Window) > 0 && startWindow > w.Window[0] {
		w.Window = w.Window[1:]
	}
	if len(w.Window) < w.Capacity {
		w.Window = append(w.Window, currentTime)
		return true
	}
	return false
}

type SlidingWindowRateLimiting struct{}

func (s *SlidingWindowRateLimiting) Allowed(request *Request) bool {
	return false
}
