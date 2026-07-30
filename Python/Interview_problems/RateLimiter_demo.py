import time
from abc import ABC, abstractmethod
from collections import deque
from typing import Optional


class RateLimitingStrategy(ABC):
    @abstractmethod
    def allowed(self, request: "Request") -> bool:
        pass


class TokenBucket:
    def __init__(self):
        self.capacity: int = 0
        self.tokens: int = 0
        self.last_refill_time: int = 0
        self.refill_rate: int = 0

    def is_allowed(self) -> bool:
        self.refill()

        if self.tokens > 0:
            self.tokens -= 1
            return True
        else:
            return False

    def refill(self):
        current_time = int(time.time() * 1000)
        time_diff = (current_time - self.last_refill_time) // 1000
        claimed_tokens = int(time_diff) * self.refill_rate
        self.tokens = min(self.capacity, self.tokens + claimed_tokens)
        self.last_refill_time = current_time


class Request:
    def __init__(self):
        self.user_id: Optional[str] = None
        self.ip: Optional[str] = None


# Akash, Nishaanth
class TokenBucketStrategy(RateLimitingStrategy):
    def __init__(self):
        self.token_buckets: dict[str, TokenBucket] = {}

    def allowed(self, request: Request) -> bool:
        bucket = self.token_buckets.get(request.user_id)

        if bucket.is_allowed():
            # its allowed do the work
            return True
        else:
            # error codes
            return False


class SlidingWindow:
    def __init__(self):
        self.capacity: int = 0
        self.window: deque = deque()
        self.window_size: Optional[int] = None

    def is_allowed(self) -> bool:
        current_time = int(time.time() * 1000)
        start_window = current_time - self.window_size
        while len(self.window) > 0 and start_window > self.window[0]:
            self.window.popleft()
        if len(self.window) < self.capacity:
            self.window.append(current_time)
            return True
        else:
            return False


class SlidingWindowRateLimiting(RateLimitingStrategy):
    def allowed(self, request: Request) -> bool:
        pass
