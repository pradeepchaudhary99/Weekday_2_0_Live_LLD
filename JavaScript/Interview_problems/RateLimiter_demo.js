class RateLimitingStrategy {
    allowed(request) {
        throw new Error("Method 'allowed()' must be implemented.");
    }
}

class TokenBucket {
    constructor() {
        this.capacity = 0;
        this.tokens = 0;
        this.lastRefillTime = 0;
        this.refillRate = 0;
    }

    isAllowed() {
        this.refill();

        if (this.tokens > 0) {
            this.tokens--;
            return true;
        } else {
            return false;
        }
    }

    refill() {
        const currentTime = Date.now();
        const timeDiff = Math.floor((currentTime - this.lastRefillTime) / 1000);
        const claimedTokens = timeDiff * this.refillRate;
        this.tokens = Math.min(this.capacity, this.tokens + claimedTokens);
        this.lastRefillTime = currentTime;
    }
}

class Request {
    constructor() {
        this.userId = null;
        this.ip = null;
    }
}

// Akash, Nishaanth
class TokenBucketStrategy extends RateLimitingStrategy {
    constructor() {
        super();
        this.tokenBuckets = new Map();
    }

    allowed(request) {
        const bucket = this.tokenBuckets.get(request.userId);

        if (bucket.isAllowed()) {
            // its allowed do the work
            return true;
        } else {
            // error codes
            return false;
        }
    }
}

class SlidingWindow {
    constructor() {
        this.capacity = 0;
        this.window = [];
        this.windowSize = null;
    }

    isAllowed() {
        const currentTime = Date.now();
        const startWindow = currentTime - this.windowSize;
        while (this.window.length > 0 && startWindow > this.window[0]) {
            this.window.shift();
        }
        if (this.window.length < this.capacity) {
            this.window.push(currentTime);
            return true;
        } else {
            return false;
        }
    }
}

class SlidingWindowRateLimiting extends RateLimitingStrategy {
    allowed(request) {
    }
}

module.exports = {
    RateLimitingStrategy,
    TokenBucket,
    Request,
    TokenBucketStrategy,
    SlidingWindow,
    SlidingWindowRateLimiting,
};
