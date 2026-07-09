#include <iostream>
#include <memory>
#include <string>
#include <utility>

struct Notification {
    virtual void send(const std::string& message) = 0;
    virtual ~Notification() = default;
};

class SMSNotification : public Notification {
public:
    void send(const std::string& message) override {
        std::cout << "SMS is Sent: message" << message << "\n";
    }
};

class Whatsapp : public Notification {
public:
    void send(const std::string& message) override {
        std::cout << "whatsapp is Sent: message" << message << "\n";
    }
};

class Decorator : public Notification {
public:
    explicit Decorator(std::unique_ptr<Notification> wrapped_object)
        : wrapped_object(std::move(wrapped_object)) {}

protected:
    std::unique_ptr<Notification> wrapped_object;
};

class RateLimiter : public Decorator {
public:
    using Decorator::Decorator;

    void send(const std::string& message) override {
        std::cout << "Logic of Rate limiting 1000 Lines\n";
        std::cout << "Logic of Rate limiting 1000 Lines\n";
        std::cout << "Logic of Rate limiting 1000 Lines\n";
        std::cout << "Logic of Rate limiting 1000 Lines\n";
        std::cout << "Logic of Rate limiting 1000 Lines\n";
        wrapped_object->send(message);
    }
};

class LoadBalancer : public Decorator {
public:
    using Decorator::Decorator;

    void send(const std::string& message) override {
        std::cout << "Logic Load balancer 1000 Lines\n";
        wrapped_object->send(message);
    }
};

class JSonFormatter : public Decorator {
public:
    using Decorator::Decorator;

    void send(const std::string& message) override {
        std::cout << "Logic Json 1000 Lines\n";
        wrapped_object->send(message);
    }
};

class WhatsappFormtter : public Decorator {
public:
    using Decorator::Decorator;

    void send(const std::string& message) override {
        std::cout << "whatsapp formetter\n";
        wrapped_object->send(message);
    }
};

int main() {
    auto notification2 = std::make_unique<WhatsappFormtter>(
        std::make_unique<WhatsappFormtter>(
            std::make_unique<RateLimiter>(std::make_unique<Whatsapp>())));
    notification2->send("pradeep");
    return 0;
}
