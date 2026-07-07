from abc import ABC, abstractmethod


class Notification(ABC):
    @abstractmethod
    def send(self, message: str) -> None:
        pass


class NotificationSMS(Notification):
    def send(self, message: str) -> None:
        raise NotImplementedError("Unimplemented method 'send'")


class NotificationSlack(Notification):
    def send(self, message: str) -> None:
        raise NotImplementedError("Unimplemented method 'send'")


class NotificationServiceWithoutFactory:
    def send_notification(self, type_: str, message: str) -> None:
        if type_ == "SMS":
            NotificationSMS().send(message)
        elif type_ == "Slack":
            NotificationSlack().send(message)


# Simple Factory
class NotificationFactory:
    @staticmethod
    def get_instance(type_: str, message: str) -> Notification:
        if type_ == "SMS":
            return NotificationSMS()
        elif type_ == "Slack":
            return NotificationSlack()
        else:
            return NotificationSMS()


class NotificationServiceWithFactory:
    def send_notification(self, type_: str, message: str) -> None:
        notification = NotificationFactory.get_instance(type_, message)
        notification.send(message)


if __name__ == "__main__":
    pass
