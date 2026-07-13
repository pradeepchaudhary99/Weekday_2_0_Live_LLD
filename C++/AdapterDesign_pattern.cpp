#include <iostream>
#include <memory>
#include <string>

struct IPaymentProcessor {
    virtual void pay() = 0;
    virtual ~IPaymentProcessor() = default;
};

class Application : public IPaymentProcessor {
public:
    void pay() override {
        std::cout << "Legacy code" << std::endl;
    }
};

class StripePayment {
public:
    void makePayment() {
        std::cout << "Stripe is making payment" << std::endl;
    }
};

class StripeAdapter : public IPaymentProcessor {
    std::shared_ptr<StripePayment> stripePayment;
public:
    explicit StripeAdapter(std::shared_ptr<StripePayment> stripePayment)
        : stripePayment(std::move(stripePayment)) {}

    void pay() override {
        stripePayment->makePayment();
    }
};

class PayTM {
public:
    void makePaymentPaytm() {
        std::cout << "paytm karo" << std::endl;
    }
};

class PayTMAdapter : public IPaymentProcessor {
    std::shared_ptr<PayTM> paytm;
public:
    explicit PayTMAdapter(std::shared_ptr<PayTM> paytm) : paytm(std::move(paytm)) {}

    void pay() override {
        paytm->makePaymentPaytm();
    }
};

void clientCode(IPaymentProcessor& process) {
    process.pay();
}

int main() {
    auto process = std::make_unique<StripeAdapter>(std::make_shared<StripePayment>());

    process->pay();

    return 0;
}
