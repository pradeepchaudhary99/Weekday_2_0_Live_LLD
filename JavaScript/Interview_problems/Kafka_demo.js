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

class Message {
  constructor(key = null, value = null, topic = null) {
    this.key = key;
    this.value = value;
    this.topic = topic;
  }
}

class Partition {
  constructor(partitionId) {
    this.partitionId = partitionId;
  }
}

class PartitionSelectionStrategy {
  selectPartition(message, numPartitions) {
    throw new Error("selectPartition() must be implemented");
  }
}

class HashBasedStrategy extends PartitionSelectionStrategy {
  selectPartition(message, numPartitions) {
  }
}

class Topic {
  constructor(name) {
    this.name = name;
    this.partitions = [];
  }

  getPartition(partitionId) {
  }
}

class Broker {
  constructor() {
    this.topics = new Map();
  }

  createTopic(topicName) {
  }

  getTopic(topicName) {
  }
}

class Producer {
  constructor(broker, strategy) {
    this.broker = broker;
    this.strategy = strategy;
  }
}

class KafkaDemo {
}

function main() {
}

main();
