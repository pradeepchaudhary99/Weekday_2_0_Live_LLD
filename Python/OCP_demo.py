from abc import ABC, abstractmethod


class IDiscount(ABC):
    @abstractmethod
    def get_discounted_price(self, amount: int) -> float:
        pass


class Festival(IDiscount):
    discount = 0.25

    def get_discounted_price(self, amount: int) -> float:
        return amount * self.discount


class Holi(IDiscount):
    discount = 0.25

    def get_discounted_price(self, amount: int) -> float:
        return amount * self.discount


class PaymentService:
    def __init__(self, discount: IDiscount):
        self.discount = discount

    def pay(self, amount: int) -> None:
        final = self.discount.get_discounted_price(amount)
        print(final)


if __name__ == "__main__":
    service = PaymentService(Festival())  # solution is Factory Method design Pattern
