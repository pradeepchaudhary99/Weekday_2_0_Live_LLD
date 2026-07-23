#include <string>

// NotificationSystem
//
// 1. Understand the problem, ask clarifying questions
//
// 2. List down the functional and non functional requirements
//
// functional requirements:
// 1.
// 2.
// 3.
// 4.
// 5.
//
// Non-functional requirements:
// 1.
// 2.
// 3.
// 4.
//
// 3. Identifying the core entities and Relationship
//
// Identifying the Core Entity?
// Classes, Interfaces, Enums
//
// Notification
// NotificationChannel
// NotificationTypes
// ConcreateNotificationChannels{SMS, EMAIL}
//
// NotificationService
// NotificationDispatcher

enum class NotificationType {
    SMS,
    EMAIL,
    SLACK,
    WHATSAPP
};

struct Notification {
    NotificationType type;
    std::string id;
    std::string message;
};

struct NotificationChannel {
    virtual void sendNotification(const Notification& notification) = 0;
    virtual ~NotificationChannel() = default;
};

class SMSNotification : public NotificationChannel {
public:
    void sendNotification(const Notification& notification) override {
    }
};

class EmailNotification : public NotificationChannel {
public:
    void sendNotification(const Notification& notification) override {
    }
};

class SlackNotification : public NotificationChannel {
public:
    void sendNotification(const Notification& notification) override {
    }
};

class LLDQuestions {
};

// Posting Questions
//     --> go over internet... understand... functional non
//     Questions
//
//     2 Questions Everyday

// Next Class:

int main() {
    return 0;
}
