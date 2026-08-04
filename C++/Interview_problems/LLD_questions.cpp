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
    NotificationType         -- SMS, EMAIL, SLACK, WHATSAPP
    NotificationStatus        -- PENDING, SENT, FAILED
    Notification              -- id, type, recipient, message, status, attempts
    NotificationChannel       -- strategy interface: send(notification) -> bool
    Concrete channels         -- SMS/Email/WhatsApp (mock, always succeed),
                                  Slack (mock, fails a configurable number of
                                  times first, to exercise the retry path)
    NotificationDispatcher    -- registry of NotificationType -> NotificationChannel,
                                  owns the retry loop
    NotificationService       -- facade: creates a Notification and asks the
                                  dispatcher to deliver it
================================================================================
*/

#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>

enum class NotificationType { SMS, EMAIL, SLACK, WHATSAPP };

std::string notificationTypeName(NotificationType type) {
    switch (type) {
        case NotificationType::SMS: return "SMS";
        case NotificationType::EMAIL: return "EMAIL";
        case NotificationType::SLACK: return "SLACK";
        case NotificationType::WHATSAPP: return "WHATSAPP";
    }
    return "UNKNOWN";
}

enum class NotificationStatus { PENDING, SENT, FAILED };

std::string notificationStatusName(NotificationStatus status) {
    switch (status) {
        case NotificationStatus::PENDING: return "PENDING";
        case NotificationStatus::SENT: return "SENT";
        case NotificationStatus::FAILED: return "FAILED";
    }
    return "UNKNOWN";
}

namespace {
int idCounter = 0;
std::string generateId() { return "notif-" + std::to_string(++idCounter); }
}  // namespace

struct Notification {
    std::string id;
    NotificationType type;
    std::string recipient;
    std::string message;
    NotificationStatus status = NotificationStatus::PENDING;
    int attempts = 0;

    Notification(NotificationType type, std::string recipient, std::string message)
        : id(generateId()), type(type), recipient(std::move(recipient)), message(std::move(message)) {}
};

struct NotificationChannel {
    // Returns true if the channel accepted/delivered the message.
    virtual bool send(Notification& notification) = 0;
    virtual ~NotificationChannel() = default;
};

class SMSNotificationChannel : public NotificationChannel {
public:
    bool send(Notification& notification) override {
        std::cout << "[SMS] to " << notification.recipient << ": " << notification.message << std::endl;
        return true;
    }
};

class EmailNotificationChannel : public NotificationChannel {
public:
    bool send(Notification& notification) override {
        std::cout << "[EMAIL] to " << notification.recipient << ": " << notification.message << std::endl;
        return true;
    }
};

class WhatsAppNotificationChannel : public NotificationChannel {
public:
    bool send(Notification& notification) override {
        std::cout << "[WHATSAPP] to " << notification.recipient << ": " << notification.message << std::endl;
        return true;
    }
};

// Simulates a channel with a flaky downstream provider: the first
// `failuresBeforeSuccess` calls fail, after which it starts succeeding.
// This exists purely to exercise NotificationDispatcher's retry loop
// deterministically, without relying on randomness.
class SlackNotificationChannel : public NotificationChannel {
public:
    explicit SlackNotificationChannel(int failuresBeforeSuccess) : remainingFailures(failuresBeforeSuccess) {}

    bool send(Notification& notification) override {
        if (remainingFailures > 0) {
            remainingFailures--;
            std::cout << "[SLACK] delivery attempt failed (provider timeout) for " << notification.recipient
                       << std::endl;
            return false;
        }
        std::cout << "[SLACK] to " << notification.recipient << ": " << notification.message << std::endl;
        return true;
    }

private:
    int remainingFailures;
};

class NotificationDispatcher {
public:
    explicit NotificationDispatcher(int maxAttempts) : maxAttempts(maxAttempts) {}

    void registerChannel(NotificationType type, std::shared_ptr<NotificationChannel> channel) {
        channels[type] = std::move(channel);
    }

    void dispatch(Notification& notification) {
        auto it = channels.find(notification.type);
        if (it == channels.end()) {
            throw std::runtime_error("No channel registered for " + notificationTypeName(notification.type));
        }
        auto& channel = it->second;

        while (notification.attempts < maxAttempts) {
            notification.attempts++;
            bool delivered = channel->send(notification);
            if (delivered) {
                notification.status = NotificationStatus::SENT;
                return;
            }
        }
        notification.status = NotificationStatus::FAILED;
    }

private:
    int maxAttempts;
    std::unordered_map<NotificationType, std::shared_ptr<NotificationChannel>> channels;
};

class NotificationService {
public:
    explicit NotificationService(std::shared_ptr<NotificationDispatcher> dispatcher)
        : dispatcher(std::move(dispatcher)) {}

    std::shared_ptr<Notification> createAndSend(NotificationType type, const std::string& recipient,
                                                 const std::string& message) {
        auto notification = std::make_shared<Notification>(type, recipient, message);
        notifications[notification->id] = notification;
        dispatcher->dispatch(*notification);
        return notification;
    }

private:
    std::shared_ptr<NotificationDispatcher> dispatcher;
    std::unordered_map<std::string, std::shared_ptr<Notification>> notifications;
};

int main() {
    auto dispatcher = std::make_shared<NotificationDispatcher>(3);
    dispatcher->registerChannel(NotificationType::SMS, std::make_shared<SMSNotificationChannel>());
    dispatcher->registerChannel(NotificationType::EMAIL, std::make_shared<EmailNotificationChannel>());
    dispatcher->registerChannel(NotificationType::WHATSAPP, std::make_shared<WhatsAppNotificationChannel>());
    // Fails twice, then succeeds on the 3rd attempt -- within maxAttempts.
    dispatcher->registerChannel(NotificationType::SLACK, std::make_shared<SlackNotificationChannel>(2));

    NotificationService service(dispatcher);

    auto sms = service.createAndSend(NotificationType::SMS, "+1-555-0100", "Your OTP is 482913");
    auto email = service.createAndSend(NotificationType::EMAIL, "alice@example.com", "Your order has shipped");
    auto whatsapp = service.createAndSend(NotificationType::WHATSAPP, "+1-555-0200", "Your table is ready");

    std::cout << "\n-- Sending a Slack notification through a flaky channel (retries expected) --" << std::endl;
    auto slack = service.createAndSend(NotificationType::SLACK, "#alerts", "Build #482 failed");

    std::cout << "\nFinal delivery report:" << std::endl;
    for (auto& n : {sms, email, whatsapp, slack}) {
        std::cout << "  " << std::left << std::setw(9) << notificationTypeName(n->type) << "-> " << std::setw(7)
                   << notificationStatusName(n->status) << "(attempts=" << n->attempts << ")" << std::endl;
    }

    return 0;
}
