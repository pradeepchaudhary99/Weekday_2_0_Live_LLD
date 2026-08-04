"""
Functional Requirements:
    User should be able to send notifiation
    Notification system should support multiple types of channels
    notification system will support user preferences
    NS should process the notification asynchronous
    Retry failed notifications

Non-Functional Requirement:
    Error handling
    Asynchronous
    Atleast once delivery


Notification
NotificationService
NotificationDispather
NotificationChannel
    SMSNotificationChannel
    WhatsappNotificationChannel
    .....
NotificationFactory
User
"""

from __future__ import annotations

import itertools
import queue
import threading
import time
from abc import ABC, abstractmethod
from concurrent.futures import ThreadPoolExecutor
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Dict, Set


class NotificationType(Enum):
    SMS = auto()
    WHATSAPP = auto()
    EMAIL = auto()


class NotificationStatus(Enum):
    PENDING = auto()
    SENT = auto()
    FAILED = auto()


@dataclass
class User:
    id: str
    name: str


@dataclass
class Notification:
    id: str
    user: User
    recipient_id: str
    message: str
    priority: int  # lower value = higher priority
    type: NotificationType
    status: NotificationStatus = field(default=NotificationStatus.PENDING)


class NotificationChannel(ABC):
    @abstractmethod
    def send_notification(self, notification: Notification) -> bool:
        raise NotImplementedError


class SMSNotificationChannel(NotificationChannel):
    def send_notification(self, notification: Notification) -> bool:
        print(f"[SMS] to {notification.recipient_id}: {notification.message}")
        return True


class WhatsappNotificationChannel(NotificationChannel):
    def send_notification(self, notification: Notification) -> bool:
        print(f"[WhatsApp] to {notification.recipient_id}: {notification.message}")
        return True


class EmailNotificationChannel(NotificationChannel):
    def send_notification(self, notification: Notification) -> bool:
        print(f"[Email] to {notification.recipient_id}: {notification.message}")
        return True


class NotificationChannelFactory:
    def __init__(self) -> None:
        self._registry: Dict[NotificationType, NotificationChannel] = {
            NotificationType.SMS: SMSNotificationChannel(),
            NotificationType.WHATSAPP: WhatsappNotificationChannel(),
            NotificationType.EMAIL: EmailNotificationChannel(),
        }

    def get_notification_channel(self, notification_type: NotificationType) -> NotificationChannel:
        channel = self._registry.get(notification_type)
        if channel is None:
            raise ValueError(f"No channel registered for type: {notification_type}")
        return channel


class UserPreferenceService:
    def __init__(self) -> None:
        self._preferences: Dict[str, Set[NotificationType]] = {}

    def set_preferences(self, user_id: str, types: Set[NotificationType]) -> None:
        self._preferences[user_id] = types

    def get_preferences(self, user_id: str) -> Set[NotificationType]:
        return self._preferences.get(user_id, set())


class NotificationRequestQueue:
    """Priority queue keyed on Notification.priority (lower = more urgent)."""

    def __init__(self) -> None:
        self._queue: "queue.PriorityQueue[tuple]" = queue.PriorityQueue()
        self._counter = itertools.count()  # tie-breaker to avoid comparing Notification objects

    def offer(self, notification: "Notification | None") -> None:
        priority = notification.priority if notification is not None else -1
        self._queue.put((priority, next(self._counter), notification))

    def take(self) -> Notification:
        _, _, notification = self._queue.get()
        return notification


class NotificationDispatcher:
    def __init__(self) -> None:
        self._queue = NotificationRequestQueue()

    def enqueue(self, notification: "Notification | None") -> None:
        self._queue.offer(notification)

    def next_task(self) -> "Notification | None":
        return self._queue.take()


class NotificationWorker:
    MAX_RETRIES = 3

    def __init__(
        self,
        dispatcher: NotificationDispatcher,
        preference_service: UserPreferenceService,
        channel_factory: NotificationChannelFactory,
        delivery_pool: ThreadPoolExecutor,
    ) -> None:
        self._dispatcher = dispatcher
        self._preference_service = preference_service
        self._channel_factory = channel_factory
        self._delivery_pool = delivery_pool
        self._running = True

    def stop(self) -> None:
        """Enqueue a poison pill so the blocking take() in run() wakes up and exits."""
        self._running = False
        self._dispatcher.enqueue(None)  # type: ignore[arg-type]

    def run(self) -> None:
        while self._running:
            notification = self._dispatcher.next_task()
            if notification is None:  # poison pill
                break

            user_pref = self._preference_service.get_preferences(notification.recipient_id)
            channels_to_use = user_pref if user_pref else {notification.type}

            for notification_type in channels_to_use:
                self._delivery_pool.submit(self._deliver_with_retry, notification, notification_type)

    def _deliver_with_retry(self, notification: Notification, notification_type: NotificationType) -> None:
        channel = self._channel_factory.get_notification_channel(notification_type)
        for attempt in range(1, self.MAX_RETRIES + 1):
            try:
                if channel.send_notification(notification):
                    notification.status = NotificationStatus.SENT
                    return
            except Exception as exc:  # noqa: BLE001 - mirrors Java's catch(Exception)
                print(f"Attempt {attempt} failed: {exc}")

        notification.status = NotificationStatus.FAILED
        print(f"Notification {notification.id} failed after {self.MAX_RETRIES} attempts")


class NotificationService:
    def __init__(self, dispatcher: NotificationDispatcher) -> None:
        self._dispatcher = dispatcher

    def submit_notification_request(self, notification: Notification) -> bool:
        self._dispatcher.enqueue(notification)
        return True


def main() -> None:
    preference_service = UserPreferenceService()
    channel_factory = NotificationChannelFactory()
    dispatcher = NotificationDispatcher()
    notification_service = NotificationService(dispatcher)

    alice = User("u1", "Alice")
    preference_service.set_preferences("u1", {NotificationType.EMAIL, NotificationType.SMS})

    delivery_pool = ThreadPoolExecutor(max_workers=4)
    worker = NotificationWorker(dispatcher, preference_service, channel_factory, delivery_pool)
    worker_thread = threading.Thread(target=worker.run, daemon=True)
    worker_thread.start()

    notification_service.submit_notification_request(
        Notification("n1", alice, "u1", "Your order has shipped!", 1, NotificationType.EMAIL)
    )
    notification_service.submit_notification_request(
        Notification("n2", alice, "u1", "OTP: 4821", 0, NotificationType.SMS)
    )

    time.sleep(0.5)
    worker.stop()
    worker_thread.join()
    delivery_pool.shutdown(wait=True)

    print("Done.")


if __name__ == "__main__":
    main()
