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
*/

'use strict';

const NotificationType = Object.freeze({
    SMS: 'SMS',
    EMAIL: 'EMAIL',
    SLACK: 'SLACK',
    WHATSAPP: 'WHATSAPP',
});

const NotificationStatus = Object.freeze({
    PENDING: 'PENDING',
    SENT: 'SENT',
    FAILED: 'FAILED',
});

let notificationIdCounter = 0;
function nextNotificationId() {
    notificationIdCounter += 1;
    return `notif-${notificationIdCounter}`;
}

class Notification {
    constructor(type, recipient, message) {
        this.id = nextNotificationId();
        this.type = type;
        this.recipient = recipient;
        this.message = message;
        this.status = NotificationStatus.PENDING;
        this.attempts = 0;
    }
}

class NotificationChannel {
    // Returns true if the channel accepted/delivered the message.
    send(notification) {
        throw new Error('send() must be implemented by subclass');
    }
}

class SMSNotificationChannel extends NotificationChannel {
    send(notification) {
        console.log(`[SMS] to ${notification.recipient}: ${notification.message}`);
        return true;
    }
}

class EmailNotificationChannel extends NotificationChannel {
    send(notification) {
        console.log(`[EMAIL] to ${notification.recipient}: ${notification.message}`);
        return true;
    }
}

class WhatsAppNotificationChannel extends NotificationChannel {
    send(notification) {
        console.log(`[WHATSAPP] to ${notification.recipient}: ${notification.message}`);
        return true;
    }
}

// Simulates a channel with a flaky downstream provider: the first
// `failuresBeforeSuccess` calls fail, after which it starts succeeding.
// This exists purely to exercise NotificationDispatcher's retry loop
// deterministically, without relying on randomness.
class SlackNotificationChannel extends NotificationChannel {
    constructor(failuresBeforeSuccess) {
        super();
        this.remainingFailures = failuresBeforeSuccess;
    }

    send(notification) {
        if (this.remainingFailures > 0) {
            this.remainingFailures -= 1;
            console.log(`[SLACK] delivery attempt failed (provider timeout) for ${notification.recipient}`);
            return false;
        }
        console.log(`[SLACK] to ${notification.recipient}: ${notification.message}`);
        return true;
    }
}

class NotificationDispatcher {
    constructor(maxAttempts) {
        this.maxAttempts = maxAttempts;
        this.channels = new Map();
    }

    registerChannel(type, channel) {
        this.channels.set(type, channel);
    }

    dispatch(notification) {
        const channel = this.channels.get(notification.type);
        if (!channel) {
            throw new Error(`No channel registered for ${notification.type}`);
        }

        while (notification.attempts < this.maxAttempts) {
            notification.attempts += 1;
            const delivered = channel.send(notification);
            if (delivered) {
                notification.status = NotificationStatus.SENT;
                return;
            }
        }
        notification.status = NotificationStatus.FAILED;
    }
}

class NotificationService {
    constructor(dispatcher) {
        this.dispatcher = dispatcher;
        this.notifications = new Map();
    }

    createAndSend(type, recipient, message) {
        const notification = new Notification(type, recipient, message);
        this.notifications.set(notification.id, notification);
        this.dispatcher.dispatch(notification);
        return notification;
    }

    getStatus(notificationId) {
        return this.notifications.get(notificationId);
    }
}

function main() {
    const dispatcher = new NotificationDispatcher(3);
    dispatcher.registerChannel(NotificationType.SMS, new SMSNotificationChannel());
    dispatcher.registerChannel(NotificationType.EMAIL, new EmailNotificationChannel());
    dispatcher.registerChannel(NotificationType.WHATSAPP, new WhatsAppNotificationChannel());
    // Fails twice, then succeeds on the 3rd attempt -- within maxAttempts.
    dispatcher.registerChannel(NotificationType.SLACK, new SlackNotificationChannel(2));

    const service = new NotificationService(dispatcher);

    const sms = service.createAndSend(NotificationType.SMS, '+1-555-0100', 'Your OTP is 482913');
    const email = service.createAndSend(NotificationType.EMAIL, 'alice@example.com', 'Your order has shipped');
    const whatsapp = service.createAndSend(NotificationType.WHATSAPP, '+1-555-0200', 'Your table is ready');

    console.log('\n-- Sending a Slack notification through a flaky channel (retries expected) --');
    const slack = service.createAndSend(NotificationType.SLACK, '#alerts', 'Build #482 failed');

    console.log('\nFinal delivery report:');
    for (const n of [sms, email, whatsapp, slack]) {
        console.log(`  ${n.type.padEnd(8)} -> ${n.status.padEnd(6)} (attempts=${n.attempts})`);
    }
}

if (require.main === module) {
    main();
}

module.exports = {
    NotificationType,
    NotificationStatus,
    Notification,
    NotificationChannel,
    SMSNotificationChannel,
    EmailNotificationChannel,
    WhatsAppNotificationChannel,
    SlackNotificationChannel,
    NotificationDispatcher,
    NotificationService,
};
