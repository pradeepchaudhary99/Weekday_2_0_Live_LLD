from abc import ABC, abstractmethod


class Notification(ABC):
    @abstractmethod
    def send(self, message: str) -> None:
        pass


class SMSNotification(Notification):
    def send(self, message: str) -> None:
        print(f"SMS is Sent: message{message}")


class Whatsapp(Notification):
    def send(self, message: str) -> None:
        print(f"whatsapp is Sent: message{message}")


class Decorator(Notification, ABC):
    def __init__(self, wrapped_object: Notification):
        self.wrapped_object = wrapped_object


class RateLimiter(Decorator):
    def send(self, message: str) -> None:
        print("Logic of Rate limiting 1000 Lines")
        print("Logic of Rate limiting 1000 Lines")
        print("Logic of Rate limiting 1000 Lines")
        print("Logic of Rate limiting 1000 Lines")
        print("Logic of Rate limiting 1000 Lines")
        self.wrapped_object.send(message)


class LoadBalancer(Decorator):
    def send(self, message: str) -> None:
        print("Logic Load balancer 1000 Lines")
        self.wrapped_object.send(message)


class JSonFormatter(Decorator):
    def send(self, message: str) -> None:
        print("Logic Json 1000 Lines")
        self.wrapped_object.send(message)


class WhatsappFormtter(Decorator):
    def send(self, message: str) -> None:
        print("whatsapp formetter")
        self.wrapped_object.send(message)


if __name__ == "__main__":
    notification2 = WhatsappFormtter(WhatsappFormtter(RateLimiter(Whatsapp())))
    notification2.send("pradeep")
