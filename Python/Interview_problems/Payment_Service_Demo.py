"""
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
    fired the task and threw the future away -- waits on the future so
    create_payment can actually return the outcome to the caller.

Core Entities:
    PaymentRequest / PaymentResponse / PaymentType / PaymentStatus
    PaymentMethod (Card / UPI / NetBanking)
    PaymentGateway (RazorPay / Stripe) / PaymentGatewayDecorator / RetryLogic
    IdempotencyManager
    PaymentService
================================================================================
"""

import threading
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from enum import Enum, auto
from typing import Dict, Optional


class PaymentType(Enum):
    CARD = auto()
    UPI = auto()
    NET_BANKING = auto()
    WALLET = auto()


class PaymentStatus(Enum):
    SUCCESS = auto()
    FAILED = auto()
    DUPLICATE = auto()


class PaymentRequest:
    def __init__(self, idempotency_key: str, user_id: str, amount: float,
                 payment_type: PaymentType, details: Dict[str, str]):
        self.idempotency_key = idempotency_key
        self.user_id = user_id
        self.amount = amount
        self.type = payment_type
        self.details = details


class PaymentResponse:
    def __init__(self, payment_id: Optional[str], status: PaymentStatus, message: str):
        self.payment_id = payment_id
        self.status = status
        self.message = message

    def __repr__(self):
        return f"PaymentResponse(payment_id={self.payment_id}, status={self.status.name}, message={self.message})"


class PaymentMethod(ABC):
    @abstractmethod
    def validate(self, request: PaymentRequest) -> bool:
        raise NotImplementedError


class CardPaymentMethod(PaymentMethod):
    def validate(self, request: PaymentRequest) -> bool:
        card_number = request.details.get("cardNumber")
        return card_number is not None and len(card_number) == 16


class UpiPaymentMethod(PaymentMethod):
    def validate(self, request: PaymentRequest) -> bool:
        upi_id = request.details.get("upiId")
        return upi_id is not None and "@" in upi_id


class NetBankingPaymentMethod(PaymentMethod):
    def validate(self, request: PaymentRequest) -> bool:
        bank_code = request.details.get("bankCode")
        return bool(bank_code)


class PaymentGateway(ABC):
    @abstractmethod
    def process(self, request: PaymentRequest) -> PaymentResponse:
        raise NotImplementedError


class RazorPayGateway(PaymentGateway):
    """Simulates a gateway that fails the first attempt for a given
    idempotency key and succeeds on every attempt after that -- enough to
    exercise RetryLogic deterministically, without relying on real network
    flakiness."""

    def __init__(self):
        self._attempts: Dict[str, int] = {}
        self._lock = threading.Lock()

    def process(self, request: PaymentRequest) -> PaymentResponse:
        with self._lock:
            attempt = self._attempts.get(request.idempotency_key, 0) + 1
            self._attempts[request.idempotency_key] = attempt
        if attempt == 1:
            return PaymentResponse(None, PaymentStatus.FAILED, "RazorPay: transient gateway error (attempt 1)")
        return PaymentResponse(f"razorpay-{request.idempotency_key}", PaymentStatus.SUCCESS,
                                f"RazorPay: payment captured (attempt {attempt})")


class StripeGateway(PaymentGateway):
    """A second, always-succeeding gateway -- demonstrates that
    PaymentService only depends on the PaymentGateway interface, so the
    processor behind it is pluggable."""

    def process(self, request: PaymentRequest) -> PaymentResponse:
        return PaymentResponse(f"stripe-{request.idempotency_key}", PaymentStatus.SUCCESS, "Stripe: payment captured")


class PaymentGatewayDecorator(PaymentGateway):
    def __init__(self, gateway: PaymentGateway):
        self.gateway = gateway


class RetryLogic(PaymentGatewayDecorator):
    def __init__(self, gateway: PaymentGateway, max_attempts: int):
        super().__init__(gateway)
        self.max_attempts = max_attempts

    def process(self, request: PaymentRequest) -> PaymentResponse:
        last_response = None
        for _ in range(self.max_attempts):
            last_response = self.gateway.process(request)
            if last_response.status == PaymentStatus.SUCCESS:
                return last_response
        return last_response


class IdempotencyManager:
    """key -> the response that was returned the first time that key was seen."""

    def __init__(self):
        self._cache: Dict[str, PaymentResponse] = {}
        self._lock = threading.Lock()

    def get_cached(self, idempotency_key: str) -> Optional[PaymentResponse]:
        with self._lock:
            return self._cache.get(idempotency_key)

    def record(self, idempotency_key: str, response: PaymentResponse) -> None:
        with self._lock:
            self._cache[idempotency_key] = response


class PaymentService:
    def __init__(self, gateway: PaymentGateway):
        self._methods: Dict[PaymentType, PaymentMethod] = {
            PaymentType.CARD: CardPaymentMethod(),
            PaymentType.UPI: UpiPaymentMethod(),
            PaymentType.NET_BANKING: NetBankingPaymentMethod(),
        }
        self._gateway = gateway
        self._idempotency_manager = IdempotencyManager()
        self._executor = ThreadPoolExecutor(max_workers=4)

    def create_payment(self, request: PaymentRequest) -> PaymentResponse:
        cached = self._idempotency_manager.get_cached(request.idempotency_key)
        if cached is not None:
            return PaymentResponse(
                cached.payment_id, PaymentStatus.DUPLICATE,
                f"Duplicate request for idempotency key {request.idempotency_key}; returning cached result")

        method = self._methods.get(request.type)
        if method is None or not method.validate(request):
            return PaymentResponse(None, PaymentStatus.FAILED, f"Validation failed for {request.type.name}")

        future = self._executor.submit(self._gateway.process, request)
        response = future.result()
        self._idempotency_manager.record(request.idempotency_key, response)
        return response

    def shutdown(self) -> None:
        self._executor.shutdown()


def main() -> None:
    service = PaymentService(RetryLogic(RazorPayGateway(), 3))

    print("1) Valid card payment (gateway fails attempt #1, RetryLogic recovers it):")
    r1 = service.create_payment(PaymentRequest("key-1", "user-1", 250.00, PaymentType.CARD,
                                                {"cardNumber": "4111111111111111"}))
    print(f"   {r1}")

    print("\n2) Same idempotency key resubmitted (must not charge again):")
    r2 = service.create_payment(PaymentRequest("key-1", "user-1", 250.00, PaymentType.CARD,
                                                {"cardNumber": "4111111111111111"}))
    print(f"   {r2}")

    print("\n3) Invalid UPI id (missing '@'):")
    r3 = service.create_payment(PaymentRequest("key-2", "user-2", 75.00, PaymentType.UPI,
                                                {"upiId": "not-a-valid-upi-id"}))
    print(f"   {r3}")

    print("\n4) Valid UPI payment on a new idempotency key (also recovers via retry):")
    r4 = service.create_payment(PaymentRequest("key-3", "user-2", 75.00, PaymentType.UPI,
                                                {"upiId": "user2@upi"}))
    print(f"   {r4}")

    service.shutdown()

    print("\n5) Swapping in Stripe for a net-banking payment (gateway is pluggable):")
    stripe_service = PaymentService(RetryLogic(StripeGateway(), 3))
    r5 = stripe_service.create_payment(PaymentRequest("key-4", "user-3", 500.00, PaymentType.NET_BANKING,
                                                        {"bankCode": "HDFC0001"}))
    print(f"   {r5}")
    stripe_service.shutdown()


if __name__ == "__main__":
    main()
