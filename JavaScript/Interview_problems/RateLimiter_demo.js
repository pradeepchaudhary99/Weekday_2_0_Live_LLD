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
    2. Thread-safety: Node is single-threaded per event-loop tick, so a
       synchronous `allowed()` call can never interleave with another one --
       there's nothing to lock. The design still keeps state cleanly
       per-strategy so it would port to a worker-thread model unchanged.
    3. Extensibility: adding a new algorithm (e.g. leaky bucket, fixed
       window) should mean adding a new class, not editing existing ones.

Design (Strategy pattern):
    RateLimitingStrategy is the abstraction every algorithm implements.
    Each strategy owns one bucket/window PER USER, created lazily on first
    use -- this is what the original stub got wrong: it looked up a bucket
    for a brand-new user and called a method on the resulting undefined,
    which throws immediately. A Map.get-or-create fixes that.

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

class Request {
    constructor(userId, ip) {
        this.userId = userId;
        this.ip = ip;
    }
}

class RateLimitingStrategy {
    allowed(request) {
        throw new Error("Method 'allowed()' must be implemented.");
    }
}

// One bucket per user.
class TokenBucket {
    constructor(capacity, refillRate) {
        this.capacity = capacity;
        this.tokens = capacity; // start full: first burst is allowed immediately
        this.refillRate = refillRate; // tokens added per second
        this.lastRefillTime = Date.now();
    }

    isAllowed() {
        this.refill();
        if (this.tokens > 0) {
            this.tokens--;
            return true;
        }
        return false;
    }

    refill() {
        const currentTime = Date.now();
        const timeDiffSeconds = Math.floor((currentTime - this.lastRefillTime) / 1000);
        if (timeDiffSeconds <= 0) {
            return; // avoid resetting lastRefillTime before a whole second has passed
        }
        const claimedTokens = timeDiffSeconds * this.refillRate;
        this.tokens = Math.min(this.capacity, this.tokens + claimedTokens);
        this.lastRefillTime = currentTime;
    }
}

// Akash, Nishaanth
class TokenBucketStrategy extends RateLimitingStrategy {
    constructor(capacity, refillRate) {
        super();
        this.capacity = capacity;
        this.refillRate = refillRate;
        this.tokenBuckets = new Map();
    }

    allowed(request) {
        // get-or-create lazily creates a bucket for a first-time user.
        let bucket = this.tokenBuckets.get(request.userId);
        if (!bucket) {
            bucket = new TokenBucket(this.capacity, this.refillRate);
            this.tokenBuckets.set(request.userId, bucket);
        }
        return bucket.isAllowed();
    }
}

// One sliding window per user, storing the timestamp of every accepted
// request in the trailing `windowSizeMs` milliseconds.
class SlidingWindow {
    constructor(capacity, windowSizeMs) {
        this.capacity = capacity;
        this.windowSizeMs = windowSizeMs;
        this.window = [];
    }

    isAllowed() {
        const currentTime = Date.now();
        const windowStart = currentTime - this.windowSizeMs;
        while (this.window.length > 0 && this.window[0] < windowStart) {
            this.window.shift();
        }
        if (this.window.length < this.capacity) {
            this.window.push(currentTime);
            return true;
        }
        return false;
    }
}

class SlidingWindowRateLimiting extends RateLimitingStrategy {
    constructor(capacity, windowSizeMs) {
        super();
        this.capacity = capacity;
        this.windowSizeMs = windowSizeMs;
        this.windows = new Map();
    }

    allowed(request) {
        let window = this.windows.get(request.userId);
        if (!window) {
            window = new SlidingWindow(this.capacity, this.windowSizeMs);
            this.windows.set(request.userId, window);
        }
        return window.isAllowed();
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

function fireBurst(strategy, label, userId, count) {
    console.log(`${label} -- firing ${count} back-to-back requests for ${userId}:`);
    for (let i = 1; i <= count; i++) {
        const ok = strategy.allowed(new Request(userId, "10.0.0.1"));
        console.log(`  request #${i} -> ${ok ? "ALLOWED" : "REJECTED"}`);
    }
}

async function main() {
    // Capacity 3, refills 1 token/sec: the first 3 requests burst through,
    // the 4th is rejected immediately after.
    const tokenBucket = new TokenBucketStrategy(3, 1);
    fireBurst(tokenBucket, "TokenBucket", "alice", 4);

    console.log("\nWaiting 1.1s for the token bucket to refill one token...");
    await sleep(1100);
    const afterRefill = tokenBucket.allowed(new Request("alice", "10.0.0.1"));
    console.log(`request after refill -> ${afterRefill ? "ALLOWED" : "REJECTED"}`);

    console.log();
    // A second, independent user must not be affected by alice's usage.
    fireBurst(tokenBucket, "TokenBucket", "bob", 2);

    console.log();
    // Capacity 3 requests per 500ms sliding window.
    const slidingWindow = new SlidingWindowRateLimiting(3, 500);
    fireBurst(slidingWindow, "SlidingWindow", "carol", 4);

    console.log("\nWaiting 600ms for carol's window to fully slide past...");
    await sleep(600);
    const afterSlide = slidingWindow.allowed(new Request("carol", "10.0.0.1"));
    console.log(`request after window slides -> ${afterSlide ? "ALLOWED" : "REJECTED"}`);
}

if (require.main === module) {
    main();
}

module.exports = {
    RateLimitingStrategy,
    TokenBucket,
    Request,
    TokenBucketStrategy,
    SlidingWindow,
    SlidingWindowRateLimiting,
};
