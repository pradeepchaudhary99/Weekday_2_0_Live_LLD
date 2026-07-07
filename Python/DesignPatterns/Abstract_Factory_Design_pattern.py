from abc import ABC, abstractmethod


# Product Interfaces
class IPaymentProcessor(ABC):
    @abstractmethod
    def pay(self, amount: int) -> None:
        pass


class IRefundProcessor(ABC):
    @abstractmethod
    def refund(self) -> None:
        pass


class InvoiceProcessor(ABC):
    @abstractmethod
    def pay(self) -> None:
        pass


class RazorpayPayment(IPaymentProcessor):
    def pay(self, amount: int) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class RazorpayRefund(IRefundProcessor):
    def refund(self) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class RazorpayInvoice(InvoiceProcessor):
    def pay(self) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class ClearPayPayment(IPaymentProcessor):
    def pay(self, amount: int) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class ClearPayRefund(IRefundProcessor):
    def refund(self) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class ClearPayInvoice(InvoiceProcessor):
    def pay(self) -> None:
        raise NotImplementedError("Unimplemented method 'pay'")


class IPaymentGatewayFactory(ABC):
    @abstractmethod
    def get_payment(self) -> IPaymentProcessor:
        pass

    @abstractmethod
    def get_refund(self) -> IRefundProcessor:
        pass

    @abstractmethod
    def get_invoice(self) -> InvoiceProcessor:
        pass


class RazorPayFactory(IPaymentGatewayFactory):
    def get_payment(self) -> IPaymentProcessor:
        return RazorpayPayment()

    def get_refund(self) -> IRefundProcessor:
        return RazorpayRefund()

    def get_invoice(self) -> InvoiceProcessor:
        return RazorpayInvoice()


class ClearPayFactory(IPaymentGatewayFactory):
    def get_payment(self) -> IPaymentProcessor:
        return ClearPayPayment()

    def get_refund(self) -> IRefundProcessor:
        return ClearPayRefund()

    def get_invoice(self) -> InvoiceProcessor:
        return ClearPayInvoice()


class PaymentService:
    @staticmethod
    def process_order_payment(factory: IPaymentGatewayFactory, amount: int) -> None:
        payment = factory.get_payment()
        refund = factory.get_refund()
        invoice = factory.get_invoice()


# Product Class
# Product Factory


if __name__ == "__main__":
    PaymentService.process_order_payment(ClearPayFactory(), 122)
    PaymentService.process_order_payment(RazorPayFactory(), 122)
