#include <iostream>
#include <memory>
#include <string>
#include <vector>

struct Subscriber {
    virtual void notifyMe(const std::string& videoId) = 0;
    virtual ~Subscriber() = default;
};

class Ananya : public Subscriber {
public:
    void notifyMe(const std::string& videoId) override {
        std::cout << "Ananaya: New Video uploaded" << videoId << std::endl;
    }
};

class Amol : public Subscriber {
public:
    void notifyMe(const std::string& videoId) override {
        std::cout << "Amol: New Video uploaded" << videoId << std::endl;
    }
};

class YoutubeChannel {
public:
    std::string name;
    std::vector<std::shared_ptr<Subscriber>> subscribers;

    void addSubsriber(const std::shared_ptr<Subscriber>& sub) {
        subscribers.push_back(sub);
    }

    void uploadVideo(const std::string& videoId) {
        // 10000 linee of code
        notifySubscriber(videoId);
    }

    void notifySubscriber(const std::string& videoId) {
        for (const auto& subscriber : subscribers) {
            subscriber->notifyMe(videoId);
        }
    }
};

int main() {
    YoutubeChannel channel;
    channel.addSubsriber(std::make_shared<Amol>());
    channel.addSubsriber(std::make_shared<Ananya>());

    channel.uploadVideo("how to avoid layoff in your company");
    return 0;
}
