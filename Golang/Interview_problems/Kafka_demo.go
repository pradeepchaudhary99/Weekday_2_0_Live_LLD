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

package main

type Message struct {
	Key   string
	Value string
	Topic string
}

type Partition struct {
	PartitionId int
}

type PartitionSelectionStrategy interface {
	SelectPartition(message Message, numPartitions int) int
}

type HashBasedStrategy struct{}

func (h *HashBasedStrategy) SelectPartition(message Message, numPartitions int) int {
	return 0
}

type Topic struct {
	Name       string
	Partitions []*Partition
}

func (t *Topic) GetPartition(partitionId int) *Partition {
	return nil
}

type Broker struct {
	Topics map[string]*Topic
}

func (b *Broker) CreateTopic(topicName string) {
}

func (b *Broker) GetTopic(topicName string) *Topic {
	return nil
}

type Producer struct {
	Broker   *Broker
	Strategy PartitionSelectionStrategy
}

func main() {
}
