/*
================================================================================
LLD: Payment Service
================================================================================

Functional Requirements:
    1. User should be able to create a payment.
    2. Support multiple payment methods:
       - Credit Card
       - Debit Card
       - UPI
       - Net Banking
       - Wallet
    3. Prevent duplicate payments using an idempotency key.
    4. Retry failed payments.
    5. Payment Gateway should be pluggable.

Non-Functional Requirements:
    - Service should avoid duplicate payments.
    - Security (basic input validation before a request ever reaches a
      gateway).
    - Extensible.
    - Thread safe.
    - Retry support.

Design:
    PaymentMethod (Strategy) validates a request is well-formed for its
    channel (card number, UPI id, bank code, ...) before any money moves.

    PaymentGateway (Strategy) is the pluggable boundary to an external
    processor (RazorPay, Stripe, ...). PaymentGatewayDecorator wraps a
    PaymentGateway with the same interface, and RetryLogic is a concrete
    decorator that re-attempts a failed call up to N times -- this is what
    the original stub only sketched: it called the inner gateway, checked
    for failure, and then did nothing, so a failed payment just vanished
    with no retry and no response.

    IdempotencyManager is a key -> PaymentResponse cache. A repeat request
    with a key already seen returns the cached result instead of charging
    the user twice -- this is what the original stub got wrong: it checked
    "not a duplicate" before submitting to the executor, but never checked
    the "is a duplicate" branch it also declared, and never records
    anything into the manager, so every repeat request would have
    reprocessed the payment.

    PaymentService wires a pluggable gateway, per-type payment methods, and
    the idempotency cache together. It submits gateway calls to a thread
    pool (as the original stub intended) but -- unlike the original, which
    fired the task and threw the result away -- waits on the future so
    createPayment can actually return the outcome to the caller.

Core Entities:
    PaymentRequest / PaymentResponse / PaymentType / PaymentStatus
    PaymentMethod (Card / UPI / NetBanking)
    PaymentGateway (RazorPay / Stripe) / PaymentGatewayDecorator / RetryLogic
    IdempotencyManager
    PaymentService
================================================================================
*/

#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

enum class PaymentType { CARD, UPI, NET_BANKING, WALLET };

enum class PaymentStatus { SUCCESS, FAILED, DUPLICATE };

std::string toString(PaymentStatus status) {
    switch (status) {
        case PaymentStatus::SUCCESS: return "SUCCESS";
        case PaymentStatus::FAILED: return "FAILED";
        case PaymentStatus::DUPLICATE: return "DUPLICATE";
    }
    return "UNKNOWN";
}

std::string toString(PaymentType type) {
    switch (type) {
        case PaymentType::CARD: return "CARD";
        case PaymentType::UPI: return "UPI";
        case PaymentType::NET_BANKING: return "NET_BANKING";
        case PaymentType::WALLET: return "WALLET";
    }
    return "UNKNOWN";
}

struct PaymentRequest {
    std::string idempotencyKey;
    std::string userId;
    double amount;
    PaymentType type;
    std::map<std::string, std::string> details;
};

struct PaymentResponse {
    std::optional<std::string> paymentId;
    PaymentStatus status;
    std::string message;
};

std::ostream& operator<<(std::ostream& os, const PaymentResponse& r) {
    os << "PaymentResponse(payment_id=" << (r.paymentId ? *r.paymentId : "null")
       << ", status=" << toString(r.status) << ", message=" << r.message << ")";
    return os;
}

struct PaymentMethod {
    virtual bool validate(const PaymentRequest& request) = 0;
    virtual ~PaymentMethod() = default;
};

class CardPaymentMethod : public PaymentMethod {
public:
    bool validate(const PaymentRequest& request) override {
        auto it = request.details.find("cardNumber");
        return it != request.details.end() && it->second.size() == 16;
    }
};

class UpiPaymentMethod : public PaymentMethod {
public:
    bool validate(const PaymentRequest& request) override {
        auto it = request.details.find("upiId");
        return it != request.details.end() && it->second.find('@') != std::string::npos;
    }
};

class NetBankingPaymentMethod : public PaymentMethod {
public:
    bool validate(const PaymentRequest& request) override {
        auto it = request.details.find("bankCode");
        return it != request.details.end() && !it->second.empty();
    }
};

struct PaymentGateway {
    virtual PaymentResponse process(const PaymentRequest& request) = 0;
    virtual ~PaymentGateway() = default;
};

// Simulates a gateway that fails the first attempt for a given idempotency
// key and succeeds on every attempt after that -- enough to exercise
// RetryLogic deterministically, without relying on real network flakiness.
class RazorPayGateway : public PaymentGateway {
public:
    PaymentResponse process(const PaymentRequest& request) override {
        std::lock_guard<std::mutex> lock(mutex_);
        int attempt = ++attempts_[request.idempotencyKey];
        if (attempt == 1) {
            return {std::nullopt, PaymentStatus::FAILED, "RazorPay: transient gateway error (attempt 1)"};
        }
        return {"razorpay-" + request.idempotencyKey, PaymentStatus::SUCCESS,
                "RazorPay: payment captured (attempt " + std::to_string(attempt) + ")"};
    }

private:
    std::map<std::string, int> attempts_;
    std::mutex mutex_;
};

// A second, always-succeeding gateway -- demonstrates that PaymentService
// only depends on the PaymentGateway interface, so the processor behind it
// is pluggable.
class StripeGateway : public PaymentGateway {
public:
    PaymentResponse process(const PaymentRequest& request) override {
        return {"stripe-" + request.idempotencyKey, PaymentStatus::SUCCESS, "Stripe: payment captured"};
    }
};

class PaymentGatewayDecorator : public PaymentGateway {
public:
    explicit PaymentGatewayDecorator(std::shared_ptr<PaymentGateway> gateway) : gateway_(std::move(gateway)) {}

protected:
    std::shared_ptr<PaymentGateway> gateway_;
};

class RetryLogic : public PaymentGatewayDecorator {
public:
    RetryLogic(std::shared_ptr<PaymentGateway> gateway, int maxAttempts)
        : PaymentGatewayDecorator(std::move(gateway)), maxAttempts_(maxAttempts) {}

    PaymentResponse process(const PaymentRequest& request) override {
        PaymentResponse lastResponse{std::nullopt, PaymentStatus::FAILED, "no attempts made"};
        for (int attempt = 1; attempt <= maxAttempts_; attempt++) {
            lastResponse = gateway_->process(request);
            if (lastResponse.status == PaymentStatus::SUCCESS) {
                return lastResponse;
            }
        }
        return lastResponse;
    }

private:
    int maxAttempts_;
};

// key -> the response that was returned the first time that key was seen.
class IdempotencyManager {
public:
    std::optional<PaymentResponse> getCached(const std::string& idempotencyKey) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(idempotencyKey);
        if (it == cache_.end()) return std::nullopt;
        return it->second;
    }

    void record(const std::string& idempotencyKey, const PaymentResponse& response) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_[idempotencyKey] = response;
    }

private:
    std::map<std::string, PaymentResponse> cache_;
    std::mutex mutex_;
};

class PaymentService {
public:
    explicit PaymentService(std::shared_ptr<PaymentGateway> gateway) : gateway_(std::move(gateway)) {
        methods_[PaymentType::CARD] = std::make_unique<CardPaymentMethod>();
        methods_[PaymentType::UPI] = std::make_unique<UpiPaymentMethod>();
        methods_[PaymentType::NET_BANKING] = std::make_unique<NetBankingPaymentMethod>();
    }

    PaymentResponse createPayment(const PaymentRequest& request) {
        auto cached = idempotencyManager_.getCached(request.idempotencyKey);
        if (cached.has_value()) {
            return {cached->paymentId, PaymentStatus::DUPLICATE,
                    "Duplicate request for idempotency key " + request.idempotencyKey + "; returning cached result"};
        }

        auto it = methods_.find(request.type);
        if (it == methods_.end() || !it->second->validate(request)) {
            return {std::nullopt, PaymentStatus::FAILED, "Validation failed for " + toString(request.type)};
        }

        std::future<PaymentResponse> future = std::async(std::launch::async,
                                                           [this, request]() { return gateway_->process(request); });
        PaymentResponse response = future.get();
        idempotencyManager_.record(request.idempotencyKey, response);
        return response;
    }

private:
    std::map<PaymentType, std::unique_ptr<PaymentMethod>> methods_;
    std::shared_ptr<PaymentGateway> gateway_;
    IdempotencyManager idempotencyManager_;
};

int main() {
    PaymentService service(std::make_shared<RetryLogic>(std::make_shared<RazorPayGateway>(), 3));

    std::cout << "1) Valid card payment (gateway fails attempt #1, RetryLogic recovers it):" << std::endl;
    auto r1 = service.createPayment({"key-1", "user-1", 250.00, PaymentType::CARD, {{"cardNumber", "4111111111111111"}}});
    std::cout << "   " << r1 << std::endl;

    std::cout << "\n2) Same idempotency key resubmitted (must not charge again):" << std::endl;
    auto r2 = service.createPayment({"key-1", "user-1", 250.00, PaymentType::CARD, {{"cardNumber", "4111111111111111"}}});
    std::cout << "   " << r2 << std::endl;

    std::cout << "\n3) Invalid UPI id (missing '@'):" << std::endl;
    auto r3 = service.createPayment({"key-2", "user-2", 75.00, PaymentType::UPI, {{"upiId", "not-a-valid-upi-id"}}});
    std::cout << "   " << r3 << std::endl;

    std::cout << "\n4) Valid UPI payment on a new idempotency key (also recovers via retry):" << std::endl;
    auto r4 = service.createPayment({"key-3", "user-2", 75.00, PaymentType::UPI, {{"upiId", "user2@upi"}}});
    std::cout << "   " << r4 << std::endl;

    std::cout << "\n5) Swapping in Stripe for a net-banking payment (gateway is pluggable):" << std::endl;
    PaymentService stripeService(std::make_shared<RetryLogic>(std::make_shared<StripeGateway>(), 3));
    auto r5 = stripeService.createPayment({"key-4", "user-3", 500.00, PaymentType::NET_BANKING, {{"bankCode", "HDFC0001"}}});
    std::cout << "   " << r5 << std::endl;

    return 0;
}
