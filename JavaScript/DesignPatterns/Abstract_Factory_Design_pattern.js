// Product Interfaces
class IPaymentProcessor {
    pay(amount) {
        throw new Error("Not implemented");
    }
}

class IRefundProcessor {
    refund() {
        throw new Error("Not implemented");
    }
}

class InvoiceProcessor {
    pay() {
        throw new Error("Not implemented");
    }
}

class RazorpayPayment extends IPaymentProcessor {
    pay(amount) {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class RazorpayRefund extends IRefundProcessor {
    refund() {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class RazorpayInvoice extends InvoiceProcessor {
    pay() {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class ClearPayPayment extends IPaymentProcessor {
    pay(amount) {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class ClearPayRefund extends IRefundProcessor {
    refund() {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class ClearPayInvoice extends InvoiceProcessor {
    pay() {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'pay'");
    }
}

class IPaymentGateway_Factory {
    getPayment() {
        throw new Error("Not implemented");
    }
    getRefund() {
        throw new Error("Not implemented");
    }
    getInvoice() {
        throw new Error("Not implemented");
    }
}

class RazorPayFactory extends IPaymentGateway_Factory {
    getPayment() {
        return new RazorpayPayment();
    }

    getRefund() {
        return new RazorpayRefund();
    }

    getInvoice() {
        return new RazorpayInvoice();
    }
}

class ClearPayFactory extends IPaymentGateway_Factory {
    getPayment() {
        return new ClearPayPayment();
    }

    getRefund() {
        return new ClearPayRefund();
    }

    getInvoice() {
        return new ClearPayInvoice();
    }
}

class PaymentService {
    static ProcessOrderPayment(factory, amount) {
        const payment = factory.getPayment();
        const refund = factory.getRefund();
        const invoice = factory.getInvoice();
    }
}
// Product Class
// Product Factory

class Factory_Method_designPattern {
    static main(args) {
        // NOTE: pre-existing bug preserved from Java source — PaymentService only
        // defines ProcessOrderPayment, not makeAmount. This throws a ReferenceError
        // at runtime here, mirroring the Java compile error in the original file.
        PaymentService.makeAmount(new ClearPayFactory(), 122);
        PaymentService.makeAmount(new RazorPayFactory(), 122);
    }
}

Factory_Method_designPattern.main([]);
