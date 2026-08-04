/*

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

*/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

enum class NotificationType { SMS, WHATSAPP, EMAIL };
enum class NotificationStatus { PENDING, SENT, FAILED };

struct User {
    std::string id;
    std::string name;
};

struct Notification {
    std::string id;
    std::shared_ptr<User> user;
    std::string recipientId;
    std::string message;
    int priority = 0; // lower value = higher priority
    NotificationType type;
    NotificationStatus status = NotificationStatus::PENDING;
};

struct NotificationChannel {
    virtual ~NotificationChannel() = default;
    virtual bool sendNotification(const std::shared_ptr<Notification>& notification) = 0;
};

class SMSNotificationChannel : public NotificationChannel {
public:
    bool sendNotification(const std::shared_ptr<Notification>& notification) override {
        std::cout << "[SMS] to " << notification->recipientId << ": " << notification->message << "\n";
        return true;
    }
};

class WhatsappNotificationChannel : public NotificationChannel {
public:
    bool sendNotification(const std::shared_ptr<Notification>& notification) override {
        std::cout << "[WhatsApp] to " << notification->recipientId << ": " << notification->message << "\n";
        return true;
    }
};

class EmailNotificationChannel : public NotificationChannel {
public:
    bool sendNotification(const std::shared_ptr<Notification>& notification) override {
        std::cout << "[Email] to " << notification->recipientId << ": " << notification->message << "\n";
        return true;
    }
};

class NotificationChannelFactory {
public:
    NotificationChannelFactory() {
        registry_[NotificationType::SMS] = std::make_shared<SMSNotificationChannel>();
        registry_[NotificationType::WHATSAPP] = std::make_shared<WhatsappNotificationChannel>();
        registry_[NotificationType::EMAIL] = std::make_shared<EmailNotificationChannel>();
    }

    std::shared_ptr<NotificationChannel> getNotificationChannel(NotificationType type) {
        auto it = registry_.find(type);
        if (it == registry_.end()) {
            throw std::invalid_argument("No channel registered for type");
        }
        return it->second;
    }

private:
    std::unordered_map<NotificationType, std::shared_ptr<NotificationChannel>> registry_;
};

class UserPreferenceService {
public:
    void setPreferences(const std::string& userId, std::set<NotificationType> types) {
        std::lock_guard<std::mutex> lock(mutex_);
        preferences_[userId] = std::move(types);
    }

    std::set<NotificationType> getPreferences(const std::string& userId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = preferences_.find(userId);
        if (it == preferences_.end()) {
            return {};
        }
        return it->second;
    }

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::set<NotificationType>> preferences_;
};

// Thread-safe priority queue keyed on Notification::priority (lower = more urgent).
// A null shared_ptr acts as the poison pill used to shut the worker down.
class NotificationRequestQueue {
public:
    void offer(std::shared_ptr<Notification> notification) {
        std::lock_guard<std::mutex> lock(mutex_);
        int priority = notification ? notification->priority : -1;
        heap_.push({priority, seq_++, std::move(notification)});
        cv_.notify_one();
    }

    std::shared_ptr<Notification> take() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !heap_.empty(); });
        auto top = heap_.top();
        heap_.pop();
        return top.notification;
    }

private:
    struct Entry {
        int priority;
        long seq;
        std::shared_ptr<Notification> notification;

        bool operator<(const Entry& other) const {
            if (priority != other.priority) return priority > other.priority; // min-heap on priority
            return seq > other.seq;
        }
    };

    std::mutex mutex_;
    std::condition_variable cv_;
    std::priority_queue<Entry> heap_;
    long seq_ = 0;
};

class NotificationDispatcher {
public:
    void enqueue(std::shared_ptr<Notification> notification) { queue_.offer(std::move(notification)); }
    std::shared_ptr<Notification> nextTask() { return queue_.take(); }

private:
    NotificationRequestQueue queue_;
};

class NotificationWorker {
public:
    static constexpr int MAX_RETRIES = 3;

    NotificationWorker(NotificationDispatcher& dispatcher,
                        UserPreferenceService& preferenceService,
                        NotificationChannelFactory& channelFactory)
        : dispatcher_(dispatcher), preferenceService_(preferenceService), channelFactory_(channelFactory) {}

    void run() {
        while (true) {
            auto notification = dispatcher_.nextTask();
            if (!notification) break; // poison pill

            auto userPref = preferenceService_.getPreferences(notification->recipientId);
            std::set<NotificationType> channelsToUse = userPref.empty()
                    ? std::set<NotificationType>{notification->type}
                    : userPref;

            std::vector<std::thread> deliveries;
            for (auto type : channelsToUse) {
                deliveries.emplace_back(&NotificationWorker::deliverWithRetry, this, notification, type);
            }
            for (auto& t : deliveries) t.join();
        }
    }

private:
    void deliverWithRetry(std::shared_ptr<Notification> notification, NotificationType type) {
        auto channel = channelFactory_.getNotificationChannel(type);
        for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
            try {
                if (channel->sendNotification(notification)) {
                    notification->status = NotificationStatus::SENT;
                    return;
                }
            } catch (const std::exception& e) {
                std::cout << "Attempt " << attempt << " failed: " << e.what() << "\n";
            }
        }
        notification->status = NotificationStatus::FAILED;
        std::cout << "Notification " << notification->id << " failed after " << MAX_RETRIES << " attempts\n";
    }

    NotificationDispatcher& dispatcher_;
    UserPreferenceService& preferenceService_;
    NotificationChannelFactory& channelFactory_;
};

class NotificationService {
public:
    explicit NotificationService(NotificationDispatcher& dispatcher) : dispatcher_(dispatcher) {}

    bool submitNotificationRequest(std::shared_ptr<Notification> notification) {
        dispatcher_.enqueue(std::move(notification));
        return true;
    }

private:
    NotificationDispatcher& dispatcher_;
};

int main() {
    UserPreferenceService preferenceService;
    NotificationChannelFactory channelFactory;
    NotificationDispatcher dispatcher;
    NotificationService notificationService(dispatcher);

    auto alice = std::make_shared<User>(User{"u1", "Alice"});
    preferenceService.setPreferences("u1", {NotificationType::EMAIL, NotificationType::SMS});

    NotificationWorker worker(dispatcher, preferenceService, channelFactory);
    std::thread workerThread(&NotificationWorker::run, &worker);

    auto n1 = std::make_shared<Notification>();
    n1->id = "n1";
    n1->user = alice;
    n1->recipientId = "u1";
    n1->message = "Your order has shipped!";
    n1->priority = 1;
    n1->type = NotificationType::EMAIL;
    notificationService.submitNotificationRequest(n1);

    auto n2 = std::make_shared<Notification>();
    n2->id = "n2";
    n2->user = alice;
    n2->recipientId = "u1";
    n2->message = "OTP: 4821";
    n2->priority = 0;
    n2->type = NotificationType::SMS;
    notificationService.submitNotificationRequest(n2);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    dispatcher.enqueue(nullptr); // poison pill to stop the worker
    workerThread.join();

    std::cout << "Done.\n";
    return 0;
}
