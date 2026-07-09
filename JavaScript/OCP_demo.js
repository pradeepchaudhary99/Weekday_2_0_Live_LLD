class IDiscount {
    getDiscountedPrice(amount) {
        throw new Error("Not implemented");
    }
}

class Festival extends IDiscount {
    constructor() {
        super();
        this.discount = 0.25;
    }
    getDiscountedPrice(amount) {
        return amount * this.discount;
    }
}

class Holi extends IDiscount {
    constructor() {
        super();
        this.discount = 0.25;
    }
    getDiscountedPrice(amount) {
        return amount * this.discount;
    }
}

class PaymentService {
    constructor(discount) {
        this.discount = discount;
    }
    pay(amount) {
        const final_ = this.discount.getDiscountedPrice(amount);
        console.log(final_);
    }
}

class OCP_Demo {
    static main(args) {
        const service = new PaymentService(new Festival()); // solution is Factory Method design Pattern
    }
}

OCP_Demo.main([]);
