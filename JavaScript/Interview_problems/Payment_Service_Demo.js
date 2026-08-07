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
    - Thread safe (Node's event loop makes this moot for the sync path
      below, but the design would carry over to a worker-thread model).
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
    "not a duplicate" before submitting the work, but never checked the
    "is a duplicate" branch it also declared, and never records anything
    into the manager, so every repeat request would have reprocessed the
    payment.

    PaymentService wires a pluggable gateway, per-type payment methods, and
    the idempotency cache together, and awaits the gateway's result so
    createPayment can actually return the outcome to the caller -- unlike
    the original stub, which fired the gateway call and threw the result
    away.

Core Entities:
    PaymentRequest / PaymentResponse / PaymentType / PaymentStatus
    PaymentMethod (Card / UPI / NetBanking)
    PaymentGateway (RazorPay / Stripe) / PaymentGatewayDecorator / RetryLogic
    IdempotencyManager
    PaymentService
================================================================================
*/

const PaymentType = Object.freeze({
    CARD: "CARD",
    UPI: "UPI",
    NET_BANKING: "NET_BANKING",
    WALLET: "WALLET",
});

const PaymentStatus = Object.freeze({
    SUCCESS: "SUCCESS",
    FAILED: "FAILED",
    DUPLICATE: "DUPLICATE",
});

class PaymentRequest {
    constructor(idempotencyKey, userId, amount, type, details) {
        this.idempotencyKey = idempotencyKey;
        this.userId = userId;
        this.amount = amount;
        this.type = type;
        this.details = details;
    }
}

class PaymentResponse {
    constructor(paymentId, status, message) {
        this.paymentId = paymentId;
        this.status = status;
        this.message = message;
    }

    toString() {
        return `PaymentResponse(payment_id=${this.paymentId ?? "null"}, status=${this.status}, message=${this.message})`;
    }
}

class PaymentMethod {
    validate(request) {
        throw new Error("validate() must be implemented");
    }
}

class CardPaymentMethod extends PaymentMethod {
    validate(request) {
        const cardNumber = request.details.cardNumber;
        return typeof cardNumber === "string" && cardNumber.length === 16;
    }
}

class UpiPaymentMethod extends PaymentMethod {
    validate(request) {
        const upiId = request.details.upiId;
        return typeof upiId === "string" && upiId.includes("@");
    }
}

class NetBankingPaymentMethod extends PaymentMethod {
    validate(request) {
        const bankCode = request.details.bankCode;
        return typeof bankCode === "string" && bankCode.length > 0;
    }
}

class PaymentGateway {
    async process(request) {
        throw new Error("process() must be implemented");
    }
}

// Simulates a gateway that fails the first attempt for a given idempotency
// key and succeeds on every attempt after that -- enough to exercise
// RetryLogic deterministically, without relying on real network flakiness.
class RazorPayGateway extends PaymentGateway {
    constructor() {
        super();
        this._attempts = new Map();
    }

    async process(request) {
        const attempt = (this._attempts.get(request.idempotencyKey) || 0) + 1;
        this._attempts.set(request.idempotencyKey, attempt);
        if (attempt === 1) {
            return new PaymentResponse(null, PaymentStatus.FAILED, "RazorPay: transient gateway error (attempt 1)");
        }
        return new PaymentResponse(`razorpay-${request.idempotencyKey}`, PaymentStatus.SUCCESS,
            `RazorPay: payment captured (attempt ${attempt})`);
    }
}

// A second, always-succeeding gateway -- demonstrates that PaymentService
// only depends on the PaymentGateway interface, so the processor behind it
// is pluggable.
class StripeGateway extends PaymentGateway {
    async process(request) {
        return new PaymentResponse(`stripe-${request.idempotencyKey}`, PaymentStatus.SUCCESS, "Stripe: payment captured");
    }
}

class PaymentGatewayDecorator extends PaymentGateway {
    constructor(gateway) {
        super();
        this.gateway = gateway;
    }
}

class RetryLogic extends PaymentGatewayDecorator {
    constructor(gateway, maxAttempts) {
        super(gateway);
        this.maxAttempts = maxAttempts;
    }

    async process(request) {
        let lastResponse = null;
        for (let attempt = 1; attempt <= this.maxAttempts; attempt++) {
            lastResponse = await this.gateway.process(request);
            if (lastResponse.status === PaymentStatus.SUCCESS) {
                return lastResponse;
            }
        }
        return lastResponse;
    }
}

// key -> the response that was returned the first time that key was seen.
class IdempotencyManager {
    constructor() {
        this._cache = new Map();
    }

    getCached(idempotencyKey) {
        return this._cache.get(idempotencyKey) ?? null;
    }

    record(idempotencyKey, response) {
        this._cache.set(idempotencyKey, response);
    }
}

class PaymentService {
    constructor(gateway) {
        this._methods = {
            [PaymentType.CARD]: new CardPaymentMethod(),
            [PaymentType.UPI]: new UpiPaymentMethod(),
            [PaymentType.NET_BANKING]: new NetBankingPaymentMethod(),
        };
        this._gateway = gateway;
        this._idempotencyManager = new IdempotencyManager();
    }

    async createPayment(request) {
        const cached = this._idempotencyManager.getCached(request.idempotencyKey);
        if (cached !== null) {
            return new PaymentResponse(cached.paymentId, PaymentStatus.DUPLICATE,
                `Duplicate request for idempotency key ${request.idempotencyKey}; returning cached result`);
        }

        const method = this._methods[request.type];
        if (!method || !method.validate(request)) {
            return new PaymentResponse(null, PaymentStatus.FAILED, `Validation failed for ${request.type}`);
        }

        const response = await this._gateway.process(request);
        this._idempotencyManager.record(request.idempotencyKey, response);
        return response;
    }
}

async function main() {
    const service = new PaymentService(new RetryLogic(new RazorPayGateway(), 3));

    console.log("1) Valid card payment (gateway fails attempt #1, RetryLogic recovers it):");
    const r1 = await service.createPayment(new PaymentRequest("key-1", "user-1", 250.0, PaymentType.CARD,
        { cardNumber: "4111111111111111" }));
    console.log(`   ${r1}`);

    console.log("\n2) Same idempotency key resubmitted (must not charge again):");
    const r2 = await service.createPayment(new PaymentRequest("key-1", "user-1", 250.0, PaymentType.CARD,
        { cardNumber: "4111111111111111" }));
    console.log(`   ${r2}`);

    console.log("\n3) Invalid UPI id (missing '@'):");
    const r3 = await service.createPayment(new PaymentRequest("key-2", "user-2", 75.0, PaymentType.UPI,
        { upiId: "not-a-valid-upi-id" }));
    console.log(`   ${r3}`);

    console.log("\n4) Valid UPI payment on a new idempotency key (also recovers via retry):");
    const r4 = await service.createPayment(new PaymentRequest("key-3", "user-2", 75.0, PaymentType.UPI,
        { upiId: "user2@upi" }));
    console.log(`   ${r4}`);

    console.log("\n5) Swapping in Stripe for a net-banking payment (gateway is pluggable):");
    const stripeService = new PaymentService(new RetryLogic(new StripeGateway(), 3));
    const r5 = await stripeService.createPayment(new PaymentRequest("key-4", "user-3", 500.0, PaymentType.NET_BANKING,
        { bankCode: "HDFC0001" }));
    console.log(`   ${r5}`);
}

main();
