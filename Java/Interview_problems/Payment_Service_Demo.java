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
    fired the task and threw the Future away -- waits on the Future so
    createPayment can actually return the outcome to the caller.

Core Entities:
    PaymentRequest / PaymentResponse / PaymentType / PaymentStatus
    PaymentMethod (Card / UPI / NetBanking)
    PaymentGateway (RazorPay / Stripe) / PaymentGatewayDecorator / RetryLogic
    IdempotencyManager
    PaymentService
================================================================================
*/

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicInteger;

enum PaymentType {
    CARD, UPI, NET_BANKING, WALLET
}

enum PaymentStatus {
    SUCCESS, FAILED, DUPLICATE
}

class PaymentRequest {
    final String idempotencyKey;
    final String userId;
    final double amount;
    final PaymentType type;
    final Map<String, String> details;

    PaymentRequest(String idempotencyKey, String userId, double amount, PaymentType type, Map<String, String> details) {
        this.idempotencyKey = idempotencyKey;
        this.userId = userId;
        this.amount = amount;
        this.type = type;
        this.details = details;
    }
}

class PaymentResponse {
    final String paymentId;
    final PaymentStatus status;
    final String message;

    PaymentResponse(String paymentId, PaymentStatus status, String message) {
        this.paymentId = paymentId;
        this.status = status;
        this.message = message;
    }

    @Override
    public String toString() {
        return "PaymentResponse{paymentId=" + paymentId + ", status=" + status + ", message=" + message + "}";
    }
}

interface PaymentMethod {
    boolean validate(PaymentRequest request);
}

class CardPaymentMethod implements PaymentMethod {
    @Override
    public boolean validate(PaymentRequest request) {
        String cardNumber = request.details.get("cardNumber");
        return cardNumber != null && cardNumber.length() == 16;
    }
}

class UpiPaymentMethod implements PaymentMethod {
    @Override
    public boolean validate(PaymentRequest request) {
        String upiId = request.details.get("upiId");
        return upiId != null && upiId.contains("@");
    }
}

class NetBankingPaymentMethod implements PaymentMethod {
    @Override
    public boolean validate(PaymentRequest request) {
        String bankCode = request.details.get("bankCode");
        return bankCode != null && !bankCode.isEmpty();
    }
}

interface PaymentGateway {
    PaymentResponse process(PaymentRequest request);
}

// Simulates a gateway that fails the first attempt for a given idempotency
// key and succeeds on every attempt after that -- enough to exercise
// RetryLogic deterministically, without relying on real network flakiness.
class RazorPayGateway implements PaymentGateway {
    private final Map<String, AtomicInteger> attempts = new ConcurrentHashMap<>();

    @Override
    public PaymentResponse process(PaymentRequest request) {
        int attempt = attempts.computeIfAbsent(request.idempotencyKey, k -> new AtomicInteger(0)).incrementAndGet();
        if (attempt == 1) {
            return new PaymentResponse(null, PaymentStatus.FAILED, "RazorPay: transient gateway error (attempt 1)");
        }
        return new PaymentResponse("razorpay-" + request.idempotencyKey, PaymentStatus.SUCCESS,
                "RazorPay: payment captured (attempt " + attempt + ")");
    }
}

// A second, always-succeeding gateway -- demonstrates that PaymentService
// only depends on the PaymentGateway interface, so the processor behind it
// is pluggable.
class StripeGateway implements PaymentGateway {
    @Override
    public PaymentResponse process(PaymentRequest request) {
        return new PaymentResponse("stripe-" + request.idempotencyKey, PaymentStatus.SUCCESS, "Stripe: payment captured");
    }
}

abstract class PaymentGatewayDecorator implements PaymentGateway {
    protected final PaymentGateway paymentGateway;

    PaymentGatewayDecorator(PaymentGateway paymentGateway) {
        this.paymentGateway = paymentGateway;
    }
}

class RetryLogic extends PaymentGatewayDecorator {
    private final int maxAttempts;

    RetryLogic(PaymentGateway paymentGateway, int maxAttempts) {
        super(paymentGateway);
        this.maxAttempts = maxAttempts;
    }

    @Override
    public PaymentResponse process(PaymentRequest request) {
        PaymentResponse lastResponse = null;
        for (int attempt = 1; attempt <= maxAttempts; attempt++) {
            lastResponse = paymentGateway.process(request);
            if (lastResponse.status == PaymentStatus.SUCCESS) {
                return lastResponse;
            }
        }
        return lastResponse;
    }
}

// key -> the response that was returned the first time that key was seen.
class IdempotencyManager {
    private final Map<String, PaymentResponse> cache = new ConcurrentHashMap<>();

    PaymentResponse getCached(String idempotencyKey) {
        return cache.get(idempotencyKey);
    }

    void record(String idempotencyKey, PaymentResponse response) {
        cache.put(idempotencyKey, response);
    }
}

class PaymentService {
    private final Map<PaymentType, PaymentMethod> methods = Map.of(
            PaymentType.CARD, new CardPaymentMethod(),
            PaymentType.UPI, new UpiPaymentMethod(),
            PaymentType.NET_BANKING, new NetBankingPaymentMethod());
    private final PaymentGateway gateway;
    private final IdempotencyManager idempotencyManager = new IdempotencyManager();
    private final ExecutorService executor = Executors.newFixedThreadPool(4);

    PaymentService(PaymentGateway gateway) {
        this.gateway = gateway;
    }

    PaymentResponse createPayment(PaymentRequest request) {
        PaymentResponse cached = idempotencyManager.getCached(request.idempotencyKey);
        if (cached != null) {
            return new PaymentResponse(cached.paymentId, PaymentStatus.DUPLICATE,
                    "Duplicate request for idempotency key " + request.idempotencyKey + "; returning cached result");
        }

        PaymentMethod method = methods.get(request.type);
        if (method == null || !method.validate(request)) {
            return new PaymentResponse(null, PaymentStatus.FAILED, "Validation failed for " + request.type);
        }

        try {
            Future<PaymentResponse> future = executor.submit(() -> gateway.process(request));
            PaymentResponse response = future.get();
            idempotencyManager.record(request.idempotencyKey, response);
            return response;
        } catch (InterruptedException | ExecutionException e) {
            Thread.currentThread().interrupt();
            return new PaymentResponse(null, PaymentStatus.FAILED, "Gateway error: " + e.getMessage());
        }
    }

    void shutdown() {
        executor.shutdown();
    }
}

public class Payment_Service_Demo {
    public static void main(String[] args) {
        PaymentService service = new PaymentService(new RetryLogic(new RazorPayGateway(), 3));

        System.out.println("1) Valid card payment (gateway fails attempt #1, RetryLogic recovers it):");
        PaymentResponse r1 = service.createPayment(new PaymentRequest("key-1", "user-1", 250.00, PaymentType.CARD,
                Map.of("cardNumber", "4111111111111111")));
        System.out.println("   " + r1);

        System.out.println("\n2) Same idempotency key resubmitted (must not charge again):");
        PaymentResponse r2 = service.createPayment(new PaymentRequest("key-1", "user-1", 250.00, PaymentType.CARD,
                Map.of("cardNumber", "4111111111111111")));
        System.out.println("   " + r2);

        System.out.println("\n3) Invalid UPI id (missing '@'):");
        PaymentResponse r3 = service.createPayment(new PaymentRequest("key-2", "user-2", 75.00, PaymentType.UPI,
                Map.of("upiId", "not-a-valid-upi-id")));
        System.out.println("   " + r3);

        System.out.println("\n4) Valid UPI payment on a new idempotency key (also recovers via retry):");
        PaymentResponse r4 = service.createPayment(new PaymentRequest("key-3", "user-2", 75.00, PaymentType.UPI,
                Map.of("upiId", "user2@upi")));
        System.out.println("   " + r4);

        service.shutdown();

        System.out.println("\n5) Swapping in Stripe for a net-banking payment (gateway is pluggable):");
        PaymentService stripeService = new PaymentService(new RetryLogic(new StripeGateway(), 3));
        PaymentResponse r5 = stripeService.createPayment(new PaymentRequest("key-4", "user-3", 500.00,
                PaymentType.NET_BANKING, Map.of("bankCode", "HDFC0001")));
        System.out.println("   " + r5);
        stripeService.shutdown();
    }
}
