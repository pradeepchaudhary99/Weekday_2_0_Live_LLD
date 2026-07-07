#include <memory>
#include <stdexcept>

struct IPaymentProcessor {
    virtual void pay(int amount) = 0;
    virtual ~IPaymentProcessor() = default;
};

class Razorpay : public IPaymentProcessor {
public:
    void pay(int amount) override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

class ClearPay : public IPaymentProcessor {
public:
    void pay(int amount) override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

struct IPaymentProcessorFactory {
    virtual std::unique_ptr<IPaymentProcessor> getInstance() = 0;
    virtual ~IPaymentProcessorFactory() = default;
};

class RazorPayFactory : public IPaymentProcessorFactory {
public:
    std::unique_ptr<IPaymentProcessor> getInstance() override {
        return std::make_unique<Razorpay>();
    }
};

class ClearPayFactory : public IPaymentProcessorFactory {
public:
    std::unique_ptr<IPaymentProcessor> getInstance() override {
        return std::make_unique<ClearPay>();
    }
};

class PaymentService {
public:
    static std::unique_ptr<IPaymentProcessor> paymentProcessor;

    static void makeAmount(IPaymentProcessorFactory& factory, int amount) {
        paymentProcessor = factory.getInstance();
        paymentProcessor->pay(amount);
    }
};

std::unique_ptr<IPaymentProcessor> PaymentService::paymentProcessor = nullptr;

// Product Class
// Product Factory

int main() {
    ClearPayFactory clearPayFactory;
    RazorPayFactory razorPayFactory;
    PaymentService::makeAmount(clearPayFactory, 122);
    PaymentService::makeAmount(razorPayFactory, 122);

    return 0;
}
