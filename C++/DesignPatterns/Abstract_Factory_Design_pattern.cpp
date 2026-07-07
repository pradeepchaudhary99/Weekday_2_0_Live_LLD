#include <memory>
#include <stdexcept>

// Product Interfaces
struct IPaymentProcessor {
    virtual void pay(int amount) = 0;
    virtual ~IPaymentProcessor() = default;
};

struct IRefundProcessor {
    virtual void refund() = 0;
    virtual ~IRefundProcessor() = default;
};

struct InvoiceProcessor {
    virtual void pay() = 0;
    virtual ~InvoiceProcessor() = default;
};

class RazorpayPayment : public IPaymentProcessor {
public:
    void pay(int amount) override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

class RazorpayRefund : public IRefundProcessor {
public:
    void refund() override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

class RazorpayInvoice : public InvoiceProcessor {
public:
    void pay() override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

class ClearPayPayment : public IPaymentProcessor {
public:
    void pay(int amount) override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

class ClearPayRefund : public IRefundProcessor {
public:
    void refund() override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

class ClearPayInvoice : public InvoiceProcessor {
public:
    void pay() override {
        throw std::runtime_error("Unimplemented method 'pay'");
    }
};

struct IPaymentGatewayFactory {
    virtual std::unique_ptr<IPaymentProcessor> getPayment() = 0;
    virtual std::unique_ptr<IRefundProcessor> getRefund() = 0;
    virtual std::unique_ptr<InvoiceProcessor> getInvoice() = 0;
    virtual ~IPaymentGatewayFactory() = default;
};

class RazorPayFactory : public IPaymentGatewayFactory {
public:
    std::unique_ptr<IPaymentProcessor> getPayment() override {
        return std::make_unique<RazorpayPayment>();
    }

    std::unique_ptr<IRefundProcessor> getRefund() override {
        return std::make_unique<RazorpayRefund>();
    }

    std::unique_ptr<InvoiceProcessor> getInvoice() override {
        return std::make_unique<RazorpayInvoice>();
    }
};

class ClearPayFactory : public IPaymentGatewayFactory {
public:
    std::unique_ptr<IPaymentProcessor> getPayment() override {
        return std::make_unique<ClearPayPayment>();
    }

    std::unique_ptr<IRefundProcessor> getRefund() override {
        return std::make_unique<ClearPayRefund>();
    }

    std::unique_ptr<InvoiceProcessor> getInvoice() override {
        return std::make_unique<ClearPayInvoice>();
    }
};

class PaymentService {
public:
    static void processOrderPayment(IPaymentGatewayFactory& factory, int amount) {
        std::unique_ptr<IPaymentProcessor> payment = factory.getPayment();
        std::unique_ptr<IRefundProcessor> refund = factory.getRefund();
        std::unique_ptr<InvoiceProcessor> invoice = factory.getInvoice();
    }
};

// Product Class
// Product Factory

int main() {
    ClearPayFactory clearPayFactory;
    RazorPayFactory razorPayFactory;
    PaymentService::processOrderPayment(clearPayFactory, 122);
    PaymentService::processOrderPayment(razorPayFactory, 122);

    return 0;
}
