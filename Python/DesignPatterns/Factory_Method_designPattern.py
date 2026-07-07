from abc import ABC, abstractmethod


class IPaymentProcessor(ABC):
    @abstractmethod
    def pay(self, amount: int) -> None:
        pass


class Razorpay(IPaymentProcessor):
    def pay(self, amount: int) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class ClearPay(IPaymentProcessor):
    def pay(self, amount: int) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class IPaymentProcessorFactory(ABC):
    @abstractmethod
    def get_instance(self) -> IPaymentProcessor:
        pass


class RazorPayFactory(IPaymentProcessorFactory):
    def get_instance(self) -> IPaymentProcessor:
        return Razorpay()


class ClearPayFactory(IPaymentProcessorFactory):
    def get_instance(self) -> IPaymentProcessor:
        return ClearPay()


class PaymentService:
    payment_processor: IPaymentProcessor = None

    @staticmethod
    def make_amount(factory: IPaymentProcessorFactory, amount: int) -> None:
        PaymentService.payment_processor = factory.get_instance()
        PaymentService.payment_processor.pay(amount)


# Product Class
# Product Factory


if __name__ == "__main__":
    PaymentService.make_amount(ClearPayFactory(), 122)
    PaymentService.make_amount(RazorPayFactory(), 122)
