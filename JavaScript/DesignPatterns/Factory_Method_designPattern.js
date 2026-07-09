class IPaymentProcessor {
    pay(amount) {
        throw new Error("Not implemented");
    }
}

class Razorpay extends IPaymentProcessor {
    pay(amount) {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class ClearPay extends IPaymentProcessor {
    pay(amount) {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class IPaymentProcessorFactory {
    getInstance() {
        throw new Error("Not implemented");
    }
}

class RazorPayFactory extends IPaymentProcessorFactory {
    getInstance() {
        return new Razorpay();
    }
}

class ClearPayFactory extends IPaymentProcessorFactory {
    getInstance() {
        return new ClearPay();
    }
}

class PaymentService {
    static makeAmount(factory, amount) {
        PaymentService.paymentProcessor = factory.getInstance();
        PaymentService.paymentProcessor.pay(amount);
    }
}
PaymentService.paymentProcessor = null;
// Product Class
// Product Factory

class Factory_Method_designPattern {
    static main(args) {
        PaymentService.makeAmount(new ClearPayFactory(), 122);
        PaymentService.makeAmount(new RazorPayFactory(), 122);
    }
}

Factory_Method_designPattern.main([]);
