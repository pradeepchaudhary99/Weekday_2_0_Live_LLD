import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

/*
 * ==========================================================================
 * LOW LEVEL DESIGN: Kafka (single-file)
 * ==========================================================================
 *
 * Functional Requirements:
 *   - Create a topic
 *   - A topic can have multiple partitions
 *   - Producer publishes messages to a topic
 *   - Messages are distributed across partitions (pluggable strategy)
 *   - Consumer subscribes to a topic
 *   - Consumer belongs to a consumer group; group tracks offsets per partition
 *
 * Non-Functional Requirements:
 *   - High throughput            -> append/read are O(1)/O(k), no global locks
 *   - Partition-level parallelism-> each Partition guards only its own log,
 *                                   ConcurrentHashMap for topic/offset maps
 *   - Thread-safety               -> CopyOnWriteArrayList / ConcurrentHashMap /
 *                                    AtomicLong used instead of external locks
 *                                    wherever the access pattern allows it
 *
 * Key design decisions:
 *   - PartitionSelectionStrategy is an interface (Strategy pattern) so new
 *     partitioning schemes (hash, round-robin, sticky, custom) can be added
 *     without touching Producer/Broker.
 *   - Partition is the unit of ordering + parallelism, exactly like real Kafka.
 *   - ConsumerGroup owns the OffsetManager and partition->consumer assignment,
 *     so multiple consumers in the same group share partitions and never
 *     double-process a partition (mirrors Kafka's rebalance model).
 * ==========================================================================
 */

// ---------------------------------------------------------------------------
// Message
// ---------------------------------------------------------------------------
class Message {
    private final String key;
    private final String value;
    private final String topic;
    private volatile long offset = -1;

    public Message(String key, String value, String topic) {
        this.key = key;
        this.value = value;
        this.topic = topic;
    }

    public String getKey() { return key; }
    public String getValue() { return value; }
    public String getTopic() { return topic; }
    public long getOffset() { return offset; }
    void setOffset(long offset) { this.offset = offset; }

    @Override
    public String toString() {
        return "Message{key=" + key + ", value=" + value + ", offset=" + offset + "}";
    }
}

// ---------------------------------------------------------------------------
// Partition selection strategy (Strategy pattern)
// ---------------------------------------------------------------------------
interface PartitionSelectionStrategy {
    int selectPartition(Message message, int numPartitions);
}

class HashBasedStrategy implements PartitionSelectionStrategy {
    @Override
    public int selectPartition(Message message, int numPartitions) {
        if (message.getKey() == null) {
            return ThreadLocalRandom.current().nextInt(numPartitions);
        }
        return Math.abs(message.getKey().hashCode()) % numPartitions;
    }
}

class RoundRobinStrategy implements PartitionSelectionStrategy {
    private final AtomicInteger counter = new AtomicInteger(0);

    @Override
    public int selectPartition(Message message, int numPartitions) {
        return Math.abs(counter.getAndIncrement() % numPartitions);
    }
}

// ---------------------------------------------------------------------------
// Partition: append-only log, thread-safe, unit of parallelism
// ---------------------------------------------------------------------------
class Partition {
    private final int partitionId;
    // CopyOnWriteArrayList: many concurrent readers (consumers), few writers
    // (producers), and index-based reads must stay consistent.
    private final List<Message> log = new CopyOnWriteArrayList<>();
    private final AtomicLong nextOffset = new AtomicLong(0);
    // per-partition lock keeps append ordering correct without blocking
    // other partitions -> partition-level parallelism.
    private final Object appendLock = new Object();

    public Partition(int partitionId) {
        this.partitionId = partitionId;
    }

    public long append(Message message) {
        synchronized (appendLock) {
            long offset = nextOffset.getAndIncrement();
            message.setOffset(offset);
            log.add(message);
            return offset;
        }
    }

    public List<Message> readFrom(long fromOffset, int maxMessages) {
        List<Message> result = new ArrayList<>();
        int size = log.size();
        for (long i = fromOffset; i < size && result.size() < maxMessages; i++) {
            result.add(log.get((int) i));
        }
        return result;
    }

    public int getPartitionId() { return partitionId; }
    public long getLatestOffset() { return nextOffset.get(); }
}

// ---------------------------------------------------------------------------
// Topic: named collection of partitions
// ---------------------------------------------------------------------------
class Topic {
    private final String name;
    private final List<Partition> partitions;

    public Topic(String name, int numPartitions) {
        this.name = name;
        this.partitions = new ArrayList<>(numPartitions);
        for (int i = 0; i < numPartitions; i++) {
            partitions.add(new Partition(i));
        }
    }

    public Partition getPartition(int partitionId) {
        return partitions.get(partitionId);
    }

    public int getPartitionCount() { return partitions.size(); }
    public String getName() { return name; }
}

// ---------------------------------------------------------------------------
// Broker: owns topics, routes publish() to the right partition
// ---------------------------------------------------------------------------
class Broker {
    // ConcurrentHashMap -> lock-free reads, safe concurrent topic creation
    private final Map<String, Topic> topics = new ConcurrentHashMap<>();

    public Topic createTopic(String topicName, int numPartitions) {
        return topics.computeIfAbsent(topicName, n -> new Topic(n, numPartitions));
    }

    public Topic getTopic(String topicName) {
        Topic topic = topics.get(topicName);
        if (topic == null) {
            throw new IllegalArgumentException("Unknown topic: " + topicName);
        }
        return topic;
    }

    public long publish(Message message, PartitionSelectionStrategy strategy) {
        Topic topic = getTopic(message.getTopic());
        int partitionId = strategy.selectPartition(message, topic.getPartitionCount());
        return topic.getPartition(partitionId).append(message);
    }
}

// ---------------------------------------------------------------------------
// Producer
// ---------------------------------------------------------------------------
class Producer {
    private final Broker broker;
    private final PartitionSelectionStrategy strategy;

    public Producer(Broker broker, PartitionSelectionStrategy strategy) {
        this.broker = broker;
        this.strategy = strategy;
    }

    public long send(Message message) {
        return broker.publish(message, strategy);
    }
}

// ---------------------------------------------------------------------------
// OffsetManager: <PartitionId, Offset> per consumer group
// ---------------------------------------------------------------------------
class OffsetManager {
    private final Map<Integer, Long> committedOffsets = new ConcurrentHashMap<>();

    public long getOffset(int partitionId) {
        return committedOffsets.getOrDefault(partitionId, 0L);
    }

    public void commitOffset(int partitionId, long offset) {
        committedOffsets.put(partitionId, offset);
    }
}

// ---------------------------------------------------------------------------
// ConsumerGroup: owns offsets + partition assignment across its consumers
// ---------------------------------------------------------------------------
class ConsumerGroup {
    private final String groupId;
    private final OffsetManager offsetManager = new OffsetManager();
    private final List<Consumer> consumers = new CopyOnWriteArrayList<>();
    private final Map<Integer, Consumer> assignment = new ConcurrentHashMap<>();

    public ConsumerGroup(String groupId) {
        this.groupId = groupId;
    }

    // Rebalance is the one operation that must be atomic w.r.t. membership
    // changes, so it's the only synchronized method in the group.
    public synchronized void join(Consumer consumer, Topic topic) {
        consumers.add(consumer);
        rebalance(topic);
    }

    private void rebalance(Topic topic) {
        assignment.clear();
        int numPartitions = topic.getPartitionCount();
        for (int p = 0; p < numPartitions; p++) {
            assignment.put(p, consumers.get(p % consumers.size()));
        }
    }

    public List<Integer> partitionsFor(Consumer consumer) {
        List<Integer> result = new ArrayList<>();
        for (Map.Entry<Integer, Consumer> e : assignment.entrySet()) {
            if (e.getValue() == consumer) result.add(e.getKey());
        }
        return result;
    }

    public OffsetManager getOffsetManager() { return offsetManager; }
    public String getGroupId() { return groupId; }
}

// ---------------------------------------------------------------------------
// Consumer
// ---------------------------------------------------------------------------
class Consumer {
    private final String consumerId;
    private final ConsumerGroup group;
    private final Topic topic;

    public Consumer(String consumerId, ConsumerGroup group, Topic topic) {
        this.consumerId = consumerId;
        this.group = group;
        this.topic = topic;
        group.join(this, topic);
    }

    public List<Message> poll(int maxMessagesPerPartition) {
        List<Message> polled = new ArrayList<>();
        OffsetManager offsetManager = group.getOffsetManager();
        for (int partitionId : group.partitionsFor(this)) {
            Partition partition = topic.getPartition(partitionId);
            long fromOffset = offsetManager.getOffset(partitionId);
            List<Message> messages = partition.readFrom(fromOffset, maxMessagesPerPartition);
            if (!messages.isEmpty()) {
                polled.addAll(messages);
                offsetManager.commitOffset(partitionId, fromOffset + messages.size());
            }
        }
        return polled;
    }

    public String getConsumerId() { return consumerId; }
}

// ---------------------------------------------------------------------------
// Demo
// ---------------------------------------------------------------------------
public class Kafka_demo {
    public static void main(String[] args) {
        Broker broker = new Broker();
        Topic ordersTopic = broker.createTopic("orders", 3);

        Producer producer = new Producer(broker, new HashBasedStrategy());
        for (int i = 0; i < 10; i++) {
            producer.send(new Message("user-" + (i % 3), "order-payload-" + i, "orders"));
        }

        ConsumerGroup group = new ConsumerGroup("order-processors");
        Consumer c1 = new Consumer("consumer-1", group, ordersTopic);
        Consumer c2 = new Consumer("consumer-2", group, ordersTopic);

        System.out.println("consumer-1 partitions: " + group.partitionsFor(c1));
        System.out.println("consumer-2 partitions: " + group.partitionsFor(c2));

        List<Message> batch1 = c1.poll(10);
        List<Message> batch2 = c2.poll(10);

        System.out.println("consumer-1 polled " + batch1.size() + " messages: " + batch1);
        System.out.println("consumer-2 polled " + batch2.size() + " messages: " + batch2);
    }
}