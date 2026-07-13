from abc import ABC, abstractmethod


class IPaymentProcessor(ABC):
    @abstractmethod
    def pay(self) -> None:
        pass


class Application(IPaymentProcessor):
    def pay(self) -> None:
        print("Legacy code")


class StripePayment:
    def make_payment(self) -> None:
        print("Stripe is making payment")


class StripeAdapter(IPaymentProcessor):
    def __init__(self, stripe_payment: StripePayment):
        self.stripe_payment = stripe_payment

    def pay(self) -> None:
        self.stripe_payment.make_payment()


class PayTM:
    def make_payment_paytm(self) -> None:
        print("paytm karo")


class PayTMAdapter(IPaymentProcessor):
    def __init__(self, paytm: PayTM):
        self.paytm = paytm

    def pay(self) -> None:
        self.paytm.make_payment_paytm()


def client_code(process: IPaymentProcessor) -> None:
    process.pay()


if __name__ == "__main__":
    process = StripeAdapter(StripePayment())

    process.pay()
