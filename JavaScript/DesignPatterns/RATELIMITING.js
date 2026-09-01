'use strict';

class Request {
    constructor(clientId, timestamp = Date.now() / 1000) {
        this.clientId = clientId;
        this.timestamp = timestamp;
    }
}

class IRateLimitingStrategy {
    isAllowed(request) {
        throw new Error("Not implemented");
    }
}

class TokenBucketStrategy extends IRateLimitingStrategy {
    constructor(capacity, refillRatePerSec) {
        super();
        this.capacity = capacity;
        this.refillRatePerSec = refillRatePerSec;
        this.tokens = capacity;
        this.lastRefill = Date.now() / 1000;
    }

    _refill(now) {
        const elapsed = now - this.lastRefill;
        this.tokens = Math.min(this.capacity, this.tokens + elapsed * this.refillRatePerSec);
        this.lastRefill = now;
    }

    isAllowed(request) {
        this._refill(request.timestamp);
        if (this.tokens >= 1) {
            this.tokens -= 1;
            return true;
        }
        return false;
    }
}

class SlidingWindowStrategy extends IRateLimitingStrategy {
    constructor(windowSizeSec, maxRequests) {
        super();
        this.windowSizeSec = windowSizeSec;
        this.maxRequests = maxRequests;
        this.timestamps = [];
    }

    isAllowed(request) {
        const now = request.timestamp;
        while (this.timestamps.length > 0 && this.timestamps[0] <= now - this.windowSizeSec) {
            this.timestamps.shift();
        }
        if (this.timestamps.length < this.maxRequests) {
            this.timestamps.push(now);
            return true;
        }
        return false;
    }
}

class NotificationService {
    constructor() {
        this.strategy = null;
    }

    setStrategy(strategy) {
        this.strategy = strategy;
    }

    send(request) {
        if (this.strategy === null) {
            throw new Error("No rate limiting strategy configured");
        }
        if (this.strategy.isAllowed(request)) {
            console.log(`Notification sent for client ${request.clientId}`);
        } else {
            console.log(`Rate limit exceeded for client ${request.clientId}`);
        }
    }
}

function main() {
    const service = new NotificationService();

    service.setStrategy(new TokenBucketStrategy(3, 1));
    console.log("-- Token Bucket --");
    for (let i = 0; i < 5; i++) {
        service.send(new Request("user-1"));
    }

    service.setStrategy(new SlidingWindowStrategy(60, 2));
    console.log("-- Sliding Window --");
    for (let i = 0; i < 4; i++) {
        service.send(new Request("user-2"));
    }
}

main();
