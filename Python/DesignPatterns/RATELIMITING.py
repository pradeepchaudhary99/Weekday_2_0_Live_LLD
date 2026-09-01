from abc import ABC, abstractmethod
from collections import deque
import time


class Request:
    def __init__(self, client_id: str, timestamp: float | None = None) -> None:
        self.client_id = client_id
        self.timestamp = timestamp if timestamp is not None else time.time()


class IRateLimitingStrategy(ABC):
    @abstractmethod
    def is_allowed(self, request: Request) -> bool:
        ...


class TokenBucketStrategy(IRateLimitingStrategy):
    def __init__(self, capacity: int, refill_rate_per_sec: float) -> None:
        self.capacity = capacity
        self.refill_rate_per_sec = refill_rate_per_sec
        self.tokens: float = capacity
        self.last_refill = time.time()

    def _refill(self, now: float) -> None:
        elapsed = now - self.last_refill
        self.tokens = min(self.capacity, self.tokens + elapsed * self.refill_rate_per_sec)
        self.last_refill = now

    def is_allowed(self, request: Request) -> bool:
        self._refill(request.timestamp)
        if self.tokens >= 1:
            self.tokens -= 1
            return True
        return False


class SlidingWindowStrategy(IRateLimitingStrategy):
    def __init__(self, window_size_sec: float, max_requests: int) -> None:
        self.window_size_sec = window_size_sec
        self.max_requests = max_requests
        self.timestamps: deque[float] = deque()

    def is_allowed(self, request: Request) -> bool:
        now = request.timestamp
        while self.timestamps and self.timestamps[0] <= now - self.window_size_sec:
            self.timestamps.popleft()
        if len(self.timestamps) < self.max_requests:
            self.timestamps.append(now)
            return True
        return False


class NotificationService:
    def __init__(self) -> None:
        self.strategy: IRateLimitingStrategy | None = None

    def set_strategy(self, strategy: IRateLimitingStrategy) -> None:
        self.strategy = strategy

    def send(self, request: Request) -> None:
        if self.strategy is None:
            raise RuntimeError("No rate limiting strategy configured")
        if self.strategy.is_allowed(request):
            print(f"Notification sent for client {request.client_id}")
        else:
            print(f"Rate limit exceeded for client {request.client_id}")


def main() -> None:
    service = NotificationService()

    service.set_strategy(TokenBucketStrategy(capacity=3, refill_rate_per_sec=1))
    print("-- Token Bucket --")
    for _ in range(5):
        service.send(Request("user-1"))

    service.set_strategy(SlidingWindowStrategy(window_size_sec=60, max_requests=2))
    print("-- Sliding Window --")
    for _ in range(4):
        service.send(Request("user-2"))


if __name__ == "__main__":
    main()
