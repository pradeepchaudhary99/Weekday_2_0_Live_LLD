#include <memory>
#include <stdexcept>
#include <string>

struct Notification {
    virtual void send(const std::string& message) = 0;
    virtual ~Notification() = default;
};

class NotificationSMS : public Notification {
public:
    void send(const std::string& message) override {
        throw std::runtime_error("Unimplemented method 'send'");
    }
};

class NotificationSlack : public Notification {
public:
    void send(const std::string& message) override {
        throw std::runtime_error("Unimplemented method 'send'");
    }
};

class NotificationServiceWithoutFactory {
public:
    void sendNotification(const std::string& type, const std::string& message) {
        if (type == "SMS") {
            NotificationSMS().send(message);
        } else if (type == "Slack") {
            NotificationSlack().send(message);
        }
    }
};

// Simple Factory
class NotificationFactory {
public:
    static std::unique_ptr<Notification> getInstance(const std::string& type, const std::string& message) {
        if (type == "SMS") {
            return std::make_unique<NotificationSMS>();
        } else if (type == "Slack") {
            return std::make_unique<NotificationSlack>();
        } else {
            return std::make_unique<NotificationSMS>();
        }
    }
};

class NotificationServiceWithFactory {
public:
    void sendNotification(const std::string& type, const std::string& message) {
        std::unique_ptr<Notification> notification = NotificationFactory::getInstance(type, message);
        notification->send(message);
    }
};

int main() {
    return 0;
}
