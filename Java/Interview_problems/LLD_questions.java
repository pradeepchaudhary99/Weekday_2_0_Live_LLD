/*
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
    NotificationType        -- SMS, EMAIL, SLACK, WHATSAPP
    NotificationStatus       -- PENDING, SENT, FAILED
    Notification             -- id, type, recipient, message, status, attempts
    NotificationChannel      -- strategy interface: send(Notification) -> boolean
    Concrete channels        -- SMS/Email/WhatsApp (mock, always succeed),
                                 Slack (mock, fails a configurable number of
                                 times first, to exercise the retry path)
    NotificationDispatcher   -- registry of NotificationType -> NotificationChannel,
                                 owns the retry loop
    NotificationService      -- facade: creates a Notification and asks the
                                 dispatcher to deliver it
================================================================================
*/

import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

enum NotificationType {
    SMS, EMAIL, SLACK, WHATSAPP
}

enum NotificationStatus {
    PENDING, SENT, FAILED
}

class Notification {
    final String id = UUID.randomUUID().toString();
    final NotificationType type;
    final String recipient;
    final String message;
    NotificationStatus status = NotificationStatus.PENDING;
    int attempts = 0;

    Notification(NotificationType type, String recipient, String message) {
        this.type = type;
        this.recipient = recipient;
        this.message = message;
    }
}

interface NotificationChannel {
    // Returns true if the channel accepted/delivered the message.
    boolean send(Notification notification);
}

class SMSNotificationChannel implements NotificationChannel {
    @Override
    public boolean send(Notification notification) {
        System.out.println("[SMS] to " + notification.recipient + ": " + notification.message);
        return true;
    }
}

class EmailNotificationChannel implements NotificationChannel {
    @Override
    public boolean send(Notification notification) {
        System.out.println("[EMAIL] to " + notification.recipient + ": " + notification.message);
        return true;
    }
}

class WhatsAppNotificationChannel implements NotificationChannel {
    @Override
    public boolean send(Notification notification) {
        System.out.println("[WHATSAPP] to " + notification.recipient + ": " + notification.message);
        return true;
    }
}

// Simulates a channel with a flaky downstream provider: the first
// `failuresBeforeSuccess` calls fail, after which it starts succeeding.
// This exists purely to exercise NotificationDispatcher's retry loop
// deterministically, without relying on randomness.
class SlackNotificationChannel implements NotificationChannel {
    private int remainingFailures;

    SlackNotificationChannel(int failuresBeforeSuccess) {
        this.remainingFailures = failuresBeforeSuccess;
    }

    @Override
    public boolean send(Notification notification) {
        if (remainingFailures > 0) {
            remainingFailures--;
            System.out.println("[SLACK] delivery attempt failed (provider timeout) for " + notification.recipient);
            return false;
        }
        System.out.println("[SLACK] to " + notification.recipient + ": " + notification.message);
        return true;
    }
}

class NotificationDispatcher {
    private final Map<NotificationType, NotificationChannel> channels = new HashMap<>();
    private final int maxAttempts;

    NotificationDispatcher(int maxAttempts) {
        this.maxAttempts = maxAttempts;
    }

    void registerChannel(NotificationType type, NotificationChannel channel) {
        channels.put(type, channel);
    }

    void dispatch(Notification notification) {
        NotificationChannel channel = channels.get(notification.type);
        if (channel == null) {
            throw new IllegalStateException("No channel registered for " + notification.type);
        }

        while (notification.attempts < maxAttempts) {
            notification.attempts++;
            boolean delivered = channel.send(notification);
            if (delivered) {
                notification.status = NotificationStatus.SENT;
                return;
            }
        }
        notification.status = NotificationStatus.FAILED;
    }
}

class NotificationService {
    private final NotificationDispatcher dispatcher;
    private final Map<String, Notification> notifications = new HashMap<>();

    NotificationService(NotificationDispatcher dispatcher) {
        this.dispatcher = dispatcher;
    }

    Notification createAndSend(NotificationType type, String recipient, String message) {
        Notification notification = new Notification(type, recipient, message);
        notifications.put(notification.id, notification);
        dispatcher.dispatch(notification);
        return notification;
    }

    Notification getStatus(String notificationId) {
        return notifications.get(notificationId);
    }
}

public class LLD_questions {
    public static void main(String[] args) {
        NotificationDispatcher dispatcher = new NotificationDispatcher(3);
        dispatcher.registerChannel(NotificationType.SMS, new SMSNotificationChannel());
        dispatcher.registerChannel(NotificationType.EMAIL, new EmailNotificationChannel());
        dispatcher.registerChannel(NotificationType.WHATSAPP, new WhatsAppNotificationChannel());
        // Fails twice, then succeeds on the 3rd attempt -- within maxAttempts.
        dispatcher.registerChannel(NotificationType.SLACK, new SlackNotificationChannel(2));

        NotificationService service = new NotificationService(dispatcher);

        Notification sms = service.createAndSend(NotificationType.SMS, "+1-555-0100", "Your OTP is 482913");
        Notification email = service.createAndSend(NotificationType.EMAIL, "alice@example.com", "Your order has shipped");
        Notification whatsapp = service.createAndSend(NotificationType.WHATSAPP, "+1-555-0200", "Your table is ready");

        System.out.println("\n-- Sending a Slack notification through a flaky channel (retries expected) --");
        Notification slack = service.createAndSend(NotificationType.SLACK, "#alerts", "Build #482 failed");

        System.out.println("\nFinal delivery report:");
        for (Notification n : new Notification[]{sms, email, whatsapp, slack}) {
            System.out.printf("  %-8s -> %-6s (attempts=%d)%n", n.type, n.status, n.attempts);
        }
    }
}
