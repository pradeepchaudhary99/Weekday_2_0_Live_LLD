/*

Producer
Consumer

Broker

Topics
Partition

ConsumerGroup
    -- OffsetManager
        PartitionId, Offset>


Functional Requirements:
    Create a topic
    topic can have multiple partitions
    producer publishes message to a topic
    messages are distributed across the partitions
    Consumer subscribes to a topic
    consumer belongs to a consumer group

Non-Functional Requirements:
    High Throughput
    partition-level parallelism
    Thread-safety

*/

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

struct Message {
    std::string key;
    std::string value;
    std::string topic;
};

struct Partition {
    int partitionId;
};

struct PartitionSelectionStrategy {
    virtual int selectPartition(const Message& message, int numPartitions) = 0;
    virtual ~PartitionSelectionStrategy() = default;
};

struct HashBasedStrategy : public PartitionSelectionStrategy {
    int selectPartition(const Message& message, int numPartitions) override {
        return 0;
    }
};

class Topic {
public:
    std::string name;
    std::vector<std::unique_ptr<Partition>> partitions;

    Partition* getPartition(int partitionId) {
        return nullptr;
    }
};

class Broker {
public:
    std::unordered_map<std::string, std::unique_ptr<Topic>> topics;

    void createTopic(const std::string& topicName) {
    }

    Topic* getTopic(const std::string& topicName) {
        return nullptr;
    }
};

class Producer {
public:
    Broker* broker;
    std::shared_ptr<PartitionSelectionStrategy> strategy;

    Producer(Broker* broker, std::shared_ptr<PartitionSelectionStrategy> strategy)
        : broker(broker), strategy(strategy) {}
};

class KafkaDemo {
};

int main() {
    return 0;
}
