

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
    Consumer subscirbbes to a topic 
    consumer belongs to a consumer groups  

Non-Functional Requirements: 
    High Throughput
    parition-level parallesim
    Thread-safety

*/


class Message{
    String key;
    String value;
    String topic;
}
interface PartitionSelectionStrategy;
class HashBasedStrategy implements PartitionSelectionStrategy;


class Producer{

}

class Topic{
    String name;
    List<Partition> partitions;

    Partition getPartition(int partitionId);

}

class Producer{
    Broker broker;
    PartitionSelectionStrategy strategy;
}



class Broker{
    Map<String, Topic> topics;
    createTopics();
    Topic getTopic(String topicName);
}


public class Kafka_demo {
    
}
