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
    for a brand-new user and called a method on the resulting null, which
    is a NullPointerException waiting to happen. computeIfAbsent both fixes
    that and keeps the lazy-init atomic under concurrent access.

    TokenBucket:      allows short bursts up to `capacity`, then refills at
                       a steady `refillRate` tokens/sec. Good when you want
                       to tolerate bursty-but-generally-light traffic.
    SlidingWindow:     counts requests in the trailing `windowSize`
                       milliseconds using a timestamp queue. Smooths out the
                       bursts a token bucket would allow, at the cost of
                       remembering every timestamp in the window.

Core Entities:
    Request                 -- who is calling (userId) and from where (ip)
    RateLimitingStrategy     -- the algorithm interface
    TokenBucket / TokenBucketStrategy
    SlidingWindow / SlidingWindowRateLimiting
================================================================================
*/

import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.ConcurrentHashMap;

class Request {
    String userId;
    String ip;

    Request(String userId, String ip) {
        this.userId = userId;
        this.ip = ip;
    }
}

interface RateLimitingStrategy {
    boolean allowed(Request request);
}

// One bucket per user. Not thread-safe on its own -- callers must
// synchronize per-bucket access (see TokenBucketStrategy.allowed).
class TokenBucket {
    final int capacity;
    int tokens;
    long lastRefillTime;
    final int refillRate; // tokens added per second

    TokenBucket(int capacity, int refillRate) {
        this.capacity = capacity;
        this.tokens = capacity; // start full: first burst is allowed immediately
        this.refillRate = refillRate;
        this.lastRefillTime = System.currentTimeMillis();
    }

    boolean isAllowed() {
        refill();
        if (tokens > 0) {
            tokens--;
            return true;
        }
        return false;
    }

    void refill() {
        long currentTime = System.currentTimeMillis();
        long timeDiffSeconds = (currentTime - lastRefillTime) / 1000;
        if (timeDiffSeconds <= 0) {
            return; // avoid resetting lastRefillTime before a whole second has passed
        }
        int claimedTokens = (int) timeDiffSeconds * refillRate;
        tokens = Math.min(capacity, tokens + claimedTokens);
        lastRefillTime = currentTime;
    }
}

// Akash, Nishaanth
class TokenBucketStrategy implements RateLimitingStrategy {
    private final int capacity;
    private final int refillRate;
    private final Map<String, TokenBucket> tokenBuckets = new ConcurrentHashMap<>();

    TokenBucketStrategy(int capacity, int refillRate) {
        this.capacity = capacity;
        this.refillRate = refillRate;
    }

    @Override
    public boolean allowed(Request request) {
        // computeIfAbsent both lazily creates a bucket for a first-time user
        // AND guarantees only one bucket is ever created per user under race.
        TokenBucket bucket = tokenBuckets.computeIfAbsent(
                request.userId, id -> new TokenBucket(capacity, refillRate));
        synchronized (bucket) {
            return bucket.isAllowed();
        }
    }
}

// One sliding window per user, storing the timestamp of every accepted
// request in the trailing `windowSizeMs` milliseconds.
class SlidingWindow {
    final int capacity;
    final long windowSizeMs;
    final Queue<Long> window = new LinkedList<>();

    SlidingWindow(int capacity, long windowSizeMs) {
        this.capacity = capacity;
        this.windowSizeMs = windowSizeMs;
    }

    boolean isAllowed() {
        long currentTime = System.currentTimeMillis();
        long windowStart = currentTime - windowSizeMs;
        while (!window.isEmpty() && window.peek() < windowStart) {
            window.poll();
        }
        if (window.size() < capacity) {
            window.add(currentTime);
            return true;
        }
        return false;
    }
}

class SlidingWindowRateLimiting implements RateLimitingStrategy {
    private final int capacity;
    private final long windowSizeMs;
    private final Map<String, SlidingWindow> windows = new ConcurrentHashMap<>();

    SlidingWindowRateLimiting(int capacity, long windowSizeMs) {
        this.capacity = capacity;
        this.windowSizeMs = windowSizeMs;
    }

    @Override
    public boolean allowed(Request request) {
        SlidingWindow window = windows.computeIfAbsent(
                request.userId, id -> new SlidingWindow(capacity, windowSizeMs));
        synchronized (window) {
            return window.isAllowed();
        }
    }
}

public class RateLimiter_demo {
    private static void fireBurst(RateLimitingStrategy strategy, String label, String userId, int count) {
        System.out.println(label + " -- firing " + count + " back-to-back requests for " + userId + ":");
        for (int i = 1; i <= count; i++) {
            boolean ok = strategy.allowed(new Request(userId, "10.0.0.1"));
            System.out.println("  request #" + i + " -> " + (ok ? "ALLOWED" : "REJECTED"));
        }
    }

    public static void main(String[] args) throws InterruptedException {
        // Capacity 3, refills 1 token/sec: the first 3 requests burst through,
        // the 4th is rejected immediately after.
        RateLimitingStrategy tokenBucket = new TokenBucketStrategy(3, 1);
        fireBurst(tokenBucket, "TokenBucket", "alice", 4);

        System.out.println("\nWaiting 1.1s for the token bucket to refill one token...");
        Thread.sleep(1100);
        boolean afterRefill = tokenBucket.allowed(new Request("alice", "10.0.0.1"));
        System.out.println("request after refill -> " + (afterRefill ? "ALLOWED" : "REJECTED"));

        System.out.println();
        // A second, independent user must not be affected by alice's usage.
        fireBurst(tokenBucket, "TokenBucket", "bob", 2);

        System.out.println();
        // Capacity 3 requests per 500ms sliding window.
        RateLimitingStrategy slidingWindow = new SlidingWindowRateLimiting(3, 500);
        fireBurst(slidingWindow, "SlidingWindow", "carol", 4);

        System.out.println("\nWaiting 600ms for carol's window to fully slide past...");
        Thread.sleep(600);
        boolean afterSlide = slidingWindow.allowed(new Request("carol", "10.0.0.1"));
        System.out.println("request after window slides -> " + (afterSlide ? "ALLOWED" : "REJECTED"));
    }
}


