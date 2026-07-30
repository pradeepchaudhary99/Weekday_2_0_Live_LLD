#include <chrono>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>

struct Request {
    std::string userId;
    std::string ip;
};

struct RateLimitingStrategy {
    virtual bool allowed(const Request& request) = 0;
    virtual ~RateLimitingStrategy() = default;
};

class TokenBucket {
public:
    int capacity = 0;
    int tokens = 0;
    long long lastRefillTime = 0;
    int refillRate = 0;

    bool isAllowed() {
        refill();

        if (tokens > 0) {
            tokens--;
            return true;
        } else {
            return false;
        }
    }

    void refill() {
        long long currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();
        long long timeDiff = (currentTime - lastRefillTime) / 1000;
        int claimedTokens = static_cast<int>(timeDiff) * refillRate;
        tokens = std::min(capacity, tokens + claimedTokens);
        lastRefillTime = currentTime;
    }
};

// Akash, Nishaanth
class TokenBucketStrategy : public RateLimitingStrategy {
public:
    std::unordered_map<std::string, std::shared_ptr<TokenBucket>> tokenBuckets;

    bool allowed(const Request& request) override {
        auto bucket = tokenBuckets[request.userId];

        if (bucket->isAllowed()) {
            // its allowed do the work
            return true;
        } else {
            // error codes
            return false;
        }
    }
};

class SlidingWindow {
public:
    int capacity = 0;
    std::deque<long long> window;
    long long windowSize = 0;

    bool isAllowed() {
        long long currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::system_clock::now().time_since_epoch())
                                     .count();
        long long startWindow = currentTime - windowSize;
        while (!window.empty() && startWindow > window.front()) {
            window.pop_front();
        }
        if (static_cast<int>(window.size()) < capacity) {
            window.push_back(currentTime);
            return true;
        } else {
            return false;
        }
    }
};

class SlidingWindowRateLimiting : public RateLimitingStrategy {
public:
    bool allowed(const Request& request) override {
        return false;
    }
};
