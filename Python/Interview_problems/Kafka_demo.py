"""

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

"""

from abc import ABC, abstractmethod
from typing import Dict, List, Optional


class Message:
    def __init__(self, key: str = None, value: str = None, topic: str = None):
        self.key = key
        self.value = value
        self.topic = topic


class Partition:
    def __init__(self, partition_id: int):
        self.partition_id = partition_id
        self.messages: List[Message] = []


class PartitionSelectionStrategy(ABC):
    @abstractmethod
    def select_partition(self, message: Message, num_partitions: int) -> int:
        pass


class HashBasedStrategy(PartitionSelectionStrategy):
    def select_partition(self, message: Message, num_partitions: int) -> int:
        pass


class Topic:
    def __init__(self, name: str):
        self.name = name
        self.partitions: List[Partition] = []

    def get_partition(self, partition_id: int) -> Optional[Partition]:
        pass


class Broker:
    def __init__(self):
        self.topics: Dict[str, Topic] = {}

    def create_topic(self, topic_name: str) -> None:
        pass

    def get_topic(self, topic_name: str) -> Optional[Topic]:
        pass


class Producer:
    def __init__(self, broker: Broker, strategy: PartitionSelectionStrategy):
        self.broker = broker
        self.strategy = strategy


class KafkaDemo:
    pass


if __name__ == "__main__":
    pass
