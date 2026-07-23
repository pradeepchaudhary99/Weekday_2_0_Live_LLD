from abc import ABC, abstractmethod
from dataclasses import dataclass
from enum import Enum, auto

# NotificationSystem
#
# 1. Understand the problem, ask clarifying questions
#
# 2. List down the functional and non functional requirements
#
# functional requirements:
# 1.
# 2.
# 3.
# 4.
# 5.
#
# Non-functional requirements:
# 1.
# 2.
# 3.
# 4.
#
# 3. Identifying the core entities and Relationship
#
# Identifying the Core Entity?
# Classes, Interfaces, Enums
#
# Notification
# NotificationChannel
# NotificationTypes
# ConcreateNotificationChannels{SMS, EMAIL}
#
# NotificationService
# NotificationDispatcher


class NotificationType(Enum):
    SMS = auto()
    EMAIL = auto()
    SLACK = auto()
    WHATSAPP = auto()


@dataclass
class Notification:
    type: "NotificationType" = None
    id: str = None
    message: str = None


class NotificationChannel(ABC):
    @abstractmethod
    def send_notification(self, notification: "Notification") -> None:
        raise NotImplementedError


class SMSNotification(NotificationChannel):
    def send_notification(self, notification: "Notification") -> None:
        pass


class EmailNotification(NotificationChannel):
    def send_notification(self, notification: "Notification") -> None:
        pass


class SlackNotification(NotificationChannel):
    def send_notification(self, notification: "Notification") -> None:
        pass


class LLDQuestions:
    pass


# Posting Questions
#     --> go over internet... understand... functional non
#     Questions
#
#     2 Questions Everyday

# Next Class:
