class IPaymentProcessor {
    pay() {
        throw new Error("Not implemented");
    }
}

class Application extends IPaymentProcessor {
    pay() {
        console.log("Legacy code");
    }
}

class StripePayment {
    makePayment() {
        console.log("Stripe is making payment");
    }
}

class StripeAdapter extends IPaymentProcessor {
    constructor(stripePayment) {
        super();
        this.stripePayment = stripePayment;
    }

    pay() {
        this.stripePayment.makePayment();
    }
}

class PayTM {
    makePaymentPaytm() {
        console.log("paytm karo");
    }
}

class PayTMAdapter extends IPaymentProcessor {
    constructor(paytm) {
        super();
        this.paytm = paytm;
    }

    pay() {
        this.paytm.makePaymentPaytm();
    }
}

function clientCode(process) {
    process.pay();
}

class AdapterDesign_pattern {
    static main(args) {
        const process = new StripeAdapter(new StripePayment());

        process.pay();
    }
}

AdapterDesign_pattern.main([]);
