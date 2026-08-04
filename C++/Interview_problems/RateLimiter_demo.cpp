/*
================================================================================
LLD: Rate Limiter
================================================================================

Problem: Design a rate limiter that throttles how many requests a client
(identified by userId) can make in a given time window, so no single client
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
       window) should mean adding a new class, not editing existing ones.

Design (Strategy pattern):
    RateLimitingStrategy is the abstraction every algorithm implements.
    Each strategy owns one bucket/window PER USER, created lazily on first
    use -- this is what the original stub got wrong: it looked up a bucket
    for a brand-new user and dereferenced the resulting null pointer. Here
    a lock-guarded lazy-insert both fixes that and keeps creation atomic
    under concurrent access.

    TokenBucket:      allows short bursts up to `capacity`, then refills at
                       a steady `refillRate` tokens/sec. Good when you want
                       to tolerate bursty-but-generally-light traffic.
    SlidingWindow:     counts requests in the trailing `windowSizeMs`
                       milliseconds using a timestamp queue. Smooths out the
                       bursts a token bucket would allow, at the cost of
                       remembering every timestamp in the window.

Core Entities:
    Request                  -- who is calling (userId) and from where (ip)
    RateLimitingStrategy      -- the algorithm interface
    TokenBucket / TokenBucketStrategy
    SlidingWindow / SlidingWindowRateLimiting
================================================================================
*/

#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

struct Request {
    std::string userId;
    std::string ip;
};

struct RateLimitingStrategy {
    virtual bool allowed(const Request& request) = 0;
    virtual ~RateLimitingStrategy() = default;
};

namespace {
long long nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
}  // namespace

// One bucket per user. Guarded by the strategy's per-user-map mutex, not
// thread-safe on its own.
class TokenBucket {
public:
    int capacity;
    int tokens;
    long long lastRefillTime;
    int refillRate;  // tokens added per second

    TokenBucket(int capacity, int refillRate)
        : capacity(capacity), tokens(capacity), lastRefillTime(nowMs()), refillRate(refillRate) {}

    bool isAllowed() {
        refill();
        if (tokens > 0) {
            tokens--;
            return true;
        }
        return false;
    }

    void refill() {
        long long currentTime = nowMs();
        long long timeDiffSeconds = (currentTime - lastRefillTime) / 1000;
        if (timeDiffSeconds <= 0) {
            return;  // avoid resetting lastRefillTime before a whole second has passed
        }
        int claimedTokens = static_cast<int>(timeDiffSeconds) * refillRate;
        tokens = std::min(capacity, tokens + claimedTokens);
        lastRefillTime = currentTime;
    }
};

// Akash, Nishaanth
class TokenBucketStrategy : public RateLimitingStrategy {
public:
    TokenBucketStrategy(int capacity, int refillRate) : capacity(capacity), refillRate(refillRate) {}

    bool allowed(const Request& request) override {
        std::lock_guard<std::mutex> lock(mutex);
        // emplace lazily creates a bucket for a first-time user AND, since
        // we hold the mutex, guarantees only one bucket is ever created per
        // user under race.
        auto it = tokenBuckets.find(request.userId);
        if (it == tokenBuckets.end()) {
            it = tokenBuckets.emplace(request.userId, std::make_unique<TokenBucket>(capacity, refillRate)).first;
        }
        return it->second->isAllowed();
    }

private:
    int capacity;
    int refillRate;
    std::unordered_map<std::string, std::unique_ptr<TokenBucket>> tokenBuckets;
    std::mutex mutex;
};

// One sliding window per user, storing the timestamp of every accepted
// request in the trailing `windowSizeMs` milliseconds.
class SlidingWindow {
public:
    int capacity;
    long long windowSizeMs;
    std::deque<long long> window;

    SlidingWindow(int capacity, long long windowSizeMs) : capacity(capacity), windowSizeMs(windowSizeMs) {}

    bool isAllowed() {
        long long currentTime = nowMs();
        long long windowStart = currentTime - windowSizeMs;
        while (!window.empty() && window.front() < windowStart) {
            window.pop_front();
        }
        if (static_cast<int>(window.size()) < capacity) {
            window.push_back(currentTime);
            return true;
        }
        return false;
    }
};

class SlidingWindowRateLimiting : public RateLimitingStrategy {
public:
    SlidingWindowRateLimiting(int capacity, long long windowSizeMs)
        : capacity(capacity), windowSizeMs(windowSizeMs) {}

    bool allowed(const Request& request) override {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = windows.find(request.userId);
        if (it == windows.end()) {
            it = windows.emplace(request.userId, std::make_unique<SlidingWindow>(capacity, windowSizeMs)).first;
        }
        return it->second->isAllowed();
    }

private:
    int capacity;
    long long windowSizeMs;
    std::unordered_map<std::string, std::unique_ptr<SlidingWindow>> windows;
    std::mutex mutex;
};

namespace {
void fireBurst(RateLimitingStrategy& strategy, const std::string& label, const std::string& userId, int count) {
    std::cout << label << " -- firing " << count << " back-to-back requests for " << userId << ":" << std::endl;
    for (int i = 1; i <= count; i++) {
        bool ok = strategy.allowed(Request{userId, "10.0.0.1"});
        std::cout << "  request #" << i << " -> " << (ok ? "ALLOWED" : "REJECTED") << std::endl;
    }
}
}  // namespace

int main() {
    // Capacity 3, refills 1 token/sec: the first 3 requests burst through,
    // the 4th is rejected immediately after.
    TokenBucketStrategy tokenBucket(3, 1);
    fireBurst(tokenBucket, "TokenBucket", "alice", 4);

    std::cout << "\nWaiting 1.1s for the token bucket to refill one token..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    bool afterRefill = tokenBucket.allowed(Request{"alice", "10.0.0.1"});
    std::cout << "request after refill -> " << (afterRefill ? "ALLOWED" : "REJECTED") << std::endl;

    std::cout << std::endl;
    // A second, independent user must not be affected by alice's usage.
    fireBurst(tokenBucket, "TokenBucket", "bob", 2);

    std::cout << std::endl;
    // Capacity 3 requests per 500ms sliding window.
    SlidingWindowRateLimiting slidingWindow(3, 500);
    fireBurst(slidingWindow, "SlidingWindow", "carol", 4);

    std::cout << "\nWaiting 600ms for carol's window to fully slide past..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    bool afterSlide = slidingWindow.allowed(Request{"carol", "10.0.0.1"});
    std::cout << "request after window slides -> " << (afterSlide ? "ALLOWED" : "REJECTED") << std::endl;

    return 0;
}
