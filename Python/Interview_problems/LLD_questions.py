"""
================================================================================
LLD: Notification System
================================================================================

Interview walkthrough (kept from the original notes, now answered):

1. Understand the problem, ask clarifying questions.
2. List the functional and non-functional requirements.
3. Identify the core entities and relationships.

--------------------------------------------------------------------------
Functional Requirements:
--------------------------------------------------------------------------
    1. Send a notification to a recipient through a specific channel
       (SMS, EMAIL, SLACK, WHATSAPP).
    2. New channels can be added without modifying existing channel code
       (Open/Closed Principle).
    3. A notification's delivery is tracked through explicit states:
       PENDING -> SENT, or PENDING -> FAILED after retries are exhausted.
    4. Transient channel failures are retried a bounded number of times
       before the notification is marked FAILED.

--------------------------------------------------------------------------
Non-Functional Requirements:
--------------------------------------------------------------------------
    1. Extensibility: adding WhatsApp/Slack/etc. is "write a new class",
       never "edit the dispatcher's if/else chain".
    2. Reliability: a flaky channel gets a bounded number of retries, not
       infinite retries (which could wedge the dispatcher) and not zero
       retries (which would surface transient blips as hard failures).
    3. Decoupling: NotificationService (what to send / to whom) knows
       nothing about HOW a channel delivers a message; NotificationChannel
       implementations know nothing about retry policy.

--------------------------------------------------------------------------
Core Entities (Strategy + Registry pattern):
--------------------------------------------------------------------------
    NotificationType          -- SMS, EMAIL, SLACK, WHATSAPP
    NotificationStatus         -- PENDING, SENT, FAILED
    Notification               -- id, type, recipient, message, status, attempts
    NotificationChannel        -- strategy interface: send(notification) -> bool
    Concrete channels          -- SMS/Email/WhatsApp (mock, always succeed),
                                   Slack (mock, fails a configurable number of
                                   times first, to exercise the retry path)
    NotificationDispatcher     -- registry of NotificationType -> NotificationChannel,
                                   owns the retry loop
    NotificationService        -- facade: creates a Notification and asks the
                                   dispatcher to deliver it
================================================================================
"""

from abc import ABC, abstractmethod
from enum import Enum, auto
from typing import Dict, Optional
from uuid import uuid4


class NotificationType(Enum):
    SMS = auto()
    EMAIL = auto()
    SLACK = auto()
    WHATSAPP = auto()


class NotificationStatus(Enum):
    PENDING = auto()
    SENT = auto()
    FAILED = auto()


class Notification:
    def __init__(self, type_: NotificationType, recipient: str, message: str):
        self.id = str(uuid4())
        self.type = type_
        self.recipient = recipient
        self.message = message
        self.status = NotificationStatus.PENDING
        self.attempts = 0


class NotificationChannel(ABC):
    @abstractmethod
    def send(self, notification: Notification) -> bool:
        """Returns True if the channel accepted/delivered the message."""
        raise NotImplementedError


class SMSNotificationChannel(NotificationChannel):
    def send(self, notification: Notification) -> bool:
        print(f"[SMS] to {notification.recipient}: {notification.message}")
        return True


class EmailNotificationChannel(NotificationChannel):
    def send(self, notification: Notification) -> bool:
        print(f"[EMAIL] to {notification.recipient}: {notification.message}")
        return True


class WhatsAppNotificationChannel(NotificationChannel):
    def send(self, notification: Notification) -> bool:
        print(f"[WHATSAPP] to {notification.recipient}: {notification.message}")
        return True


class SlackNotificationChannel(NotificationChannel):
    """Simulates a channel with a flaky downstream provider: the first
    `failures_before_success` calls fail, after which it starts succeeding.
    This exists purely to exercise NotificationDispatcher's retry loop
    deterministically, without relying on randomness."""

    def __init__(self, failures_before_success: int):
        self.remaining_failures = failures_before_success

    def send(self, notification: Notification) -> bool:
        if self.remaining_failures > 0:
            self.remaining_failures -= 1
            print(f"[SLACK] delivery attempt failed (provider timeout) for {notification.recipient}")
            return False
        print(f"[SLACK] to {notification.recipient}: {notification.message}")
        return True


class NotificationDispatcher:
    def __init__(self, max_attempts: int):
        self.max_attempts = max_attempts
        self.channels: Dict[NotificationType, NotificationChannel] = {}

    def register_channel(self, type_: NotificationType, channel: NotificationChannel) -> None:
        self.channels[type_] = channel

    def dispatch(self, notification: Notification) -> None:
        channel = self.channels.get(notification.type)
        if channel is None:
            raise ValueError(f"No channel registered for {notification.type}")

        while notification.attempts < self.max_attempts:
            notification.attempts += 1
            delivered = channel.send(notification)
            if delivered:
                notification.status = NotificationStatus.SENT
                return
        notification.status = NotificationStatus.FAILED


class NotificationService:
    def __init__(self, dispatcher: NotificationDispatcher):
        self.dispatcher = dispatcher
        self.notifications: Dict[str, Notification] = {}

    def create_and_send(self, type_: NotificationType, recipient: str, message: str) -> Notification:
        notification = Notification(type_, recipient, message)
        self.notifications[notification.id] = notification
        self.dispatcher.dispatch(notification)
        return notification

    def get_status(self, notification_id: str) -> Optional[Notification]:
        return self.notifications.get(notification_id)


def main() -> None:
    dispatcher = NotificationDispatcher(max_attempts=3)
    dispatcher.register_channel(NotificationType.SMS, SMSNotificationChannel())
    dispatcher.register_channel(NotificationType.EMAIL, EmailNotificationChannel())
    dispatcher.register_channel(NotificationType.WHATSAPP, WhatsAppNotificationChannel())
    # Fails twice, then succeeds on the 3rd attempt -- within max_attempts.
    dispatcher.register_channel(NotificationType.SLACK, SlackNotificationChannel(2))

    service = NotificationService(dispatcher)

    sms = service.create_and_send(NotificationType.SMS, "+1-555-0100", "Your OTP is 482913")
    email = service.create_and_send(NotificationType.EMAIL, "alice@example.com", "Your order has shipped")
    whatsapp = service.create_and_send(NotificationType.WHATSAPP, "+1-555-0200", "Your table is ready")

    print("\n-- Sending a Slack notification through a flaky channel (retries expected) --")
    slack = service.create_and_send(NotificationType.SLACK, "#alerts", "Build #482 failed")

    print("\nFinal delivery report:")
    for n in (sms, email, whatsapp, slack):
        print(f"  {n.type.name:<8} -> {n.status.name:<6} (attempts={n.attempts})")


if __name__ == "__main__":
    main()
