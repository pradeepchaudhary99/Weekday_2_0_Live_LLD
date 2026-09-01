#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

static double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

struct Request {
    std::string clientId;
    double timestamp;

    explicit Request(std::string id, double ts = nowSeconds())
        : clientId(std::move(id)), timestamp(ts) {}
};

struct IRateLimitingStrategy {
    virtual ~IRateLimitingStrategy() = default;
    virtual bool isAllowed(const Request& request) = 0;
};

class TokenBucketStrategy : public IRateLimitingStrategy {
public:
    TokenBucketStrategy(int capacity, double refillRatePerSec)
        : capacity_(capacity),
          refillRatePerSec_(refillRatePerSec),
          tokens_(capacity),
          lastRefill_(nowSeconds()) {}

    bool isAllowed(const Request& request) override {
        refill(request.timestamp);
        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }

private:
    void refill(double now) {
        double elapsed = now - lastRefill_;
        tokens_ = std::min(static_cast<double>(capacity_),
                           tokens_ + elapsed * refillRatePerSec_);
        lastRefill_ = now;
    }

    int capacity_;
    double refillRatePerSec_;
    double tokens_;
    double lastRefill_;
};

class SlidingWindowStrategy : public IRateLimitingStrategy {
public:
    SlidingWindowStrategy(double windowSizeSec, int maxRequests)
        : windowSizeSec_(windowSizeSec), maxRequests_(maxRequests) {}

    bool isAllowed(const Request& request) override {
        double now = request.timestamp;
        while (!timestamps_.empty() && timestamps_.front() <= now - windowSizeSec_) {
            timestamps_.pop_front();
        }
        if (static_cast<int>(timestamps_.size()) < maxRequests_) {
            timestamps_.push_back(now);
            return true;
        }
        return false;
    }

private:
    double windowSizeSec_;
    int maxRequests_;
    std::deque<double> timestamps_;
};

class NotificationService {
public:
    void setStrategy(std::unique_ptr<IRateLimitingStrategy> strategy) {
        strategy_ = std::move(strategy);
    }

    void send(const Request& request) {
        if (!strategy_) {
            throw std::runtime_error("No rate limiting strategy configured");
        }
        if (strategy_->isAllowed(request)) {
            std::cout << "Notification sent for client " << request.clientId << std::endl;
        } else {
            std::cout << "Rate limit exceeded for client " << request.clientId << std::endl;
        }
    }

private:
    std::unique_ptr<IRateLimitingStrategy> strategy_;
};

int main() {
    NotificationService service;

    service.setStrategy(std::make_unique<TokenBucketStrategy>(3, 1.0));
    std::cout << "-- Token Bucket --" << std::endl;
    for (int i = 0; i < 5; ++i) {
        service.send(Request("user-1"));
    }

    service.setStrategy(std::make_unique<SlidingWindowStrategy>(60.0, 2));
    std::cout << "-- Sliding Window --" << std::endl;
    for (int i = 0; i < 4; ++i) {
        service.send(Request("user-2"));
    }

    return 0;
}
