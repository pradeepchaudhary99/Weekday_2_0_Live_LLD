"""
================================================================================
LLD: Rate Limiter
================================================================================

Problem: Design a rate limiter that throttles how many requests a client
(identified by user_id) can make in a given time window, so no single client
can overwhelm the system.

Functional Requirements:
    1. Given a client identifier, decide whether the current request is
       ALLOWED or REJECTED.
    2. Support more than one throttling algorithm (token bucket, sliding
       window) behind a common interface, so the algorithm can be swapped
       without touching call sites.
    3. Limits are tracked independently per client -- one noisy user must
       never affect another user's quota.

Non-Functional Requirements:
    1. O(1) (or near O(1)) decision latency -- rate limiting sits on the hot
       path of every request.
    2. Thread-safety: multiple requests for the same/different users can
       arrive concurrently and must not corrupt shared counters.
    3. Extensibility: adding a new algorithm (e.g. leaky bucket, fixed
       window) should mean adding a new class, not editing existing ones.

Design (Strategy pattern):
    RateLimitingStrategy is the abstraction every algorithm implements.
    Each strategy owns one bucket/window PER USER, created lazily on first
    use -- this is what the original stub got wrong: it looked up a bucket
    for a brand-new user and called a method on the resulting None, which is
    an AttributeError waiting to happen. setdefault (guarded by a lock) both
    fixes that and keeps the lazy-init atomic under concurrent access.

    TokenBucket:      allows short bursts up to `capacity`, then refills at
                       a steady `refill_rate` tokens/sec. Good when you want
                       to tolerate bursty-but-generally-light traffic.
    SlidingWindow:     counts requests in the trailing `window_size_ms`
                       milliseconds using a timestamp queue. Smooths out the
                       bursts a token bucket would allow, at the cost of
                       remembering every timestamp in the window.

Core Entities:
    Request                  -- who is calling (user_id) and from where (ip)
    RateLimitingStrategy      -- the algorithm interface
    TokenBucket / TokenBucketStrategy
    SlidingWindow / SlidingWindowRateLimiting
================================================================================
"""

import threading
import time
from abc import ABC, abstractmethod
from collections import deque
from typing import Dict


class Request:
    def __init__(self, user_id: str, ip: str):
        self.user_id = user_id
        self.ip = ip


class RateLimitingStrategy(ABC):
    @abstractmethod
    def allowed(self, request: Request) -> bool:
        raise NotImplementedError


# One bucket per user. Guarded by the strategy's per-user lock, not
# thread-safe on its own.
class TokenBucket:
    def __init__(self, capacity: int, refill_rate: int):
        self.capacity = capacity
        self.tokens = capacity  # start full: first burst is allowed immediately
        self.refill_rate = refill_rate  # tokens added per second
        self.last_refill_time = time.time()

    def is_allowed(self) -> bool:
        self.refill()
        if self.tokens > 0:
            self.tokens -= 1
            return True
        return False

    def refill(self) -> None:
        current_time = time.time()
        time_diff_seconds = int(current_time - self.last_refill_time)
        if time_diff_seconds <= 0:
            return  # avoid resetting last_refill_time before a whole second has passed
        claimed_tokens = time_diff_seconds * self.refill_rate
        self.tokens = min(self.capacity, self.tokens + claimed_tokens)
        self.last_refill_time = current_time


# Akash, Nishaanth
class TokenBucketStrategy(RateLimitingStrategy):
    def __init__(self, capacity: int, refill_rate: int):
        self.capacity = capacity
        self.refill_rate = refill_rate
        self.token_buckets: Dict[str, TokenBucket] = {}
        self.lock = threading.Lock()

    def allowed(self, request: Request) -> bool:
        with self.lock:
            # setdefault lazily creates a bucket for a first-time user AND
            # guarantees only one bucket is ever created per user under race.
            bucket = self.token_buckets.setdefault(
                request.user_id, TokenBucket(self.capacity, self.refill_rate)
            )
            return bucket.is_allowed()


# One sliding window per user, storing the timestamp of every accepted
# request in the trailing `window_size_ms` milliseconds.
class SlidingWindow:
    def __init__(self, capacity: int, window_size_ms: int):
        self.capacity = capacity
        self.window_size_ms = window_size_ms
        self.window: deque = deque()

    def is_allowed(self) -> bool:
        current_time = time.time() * 1000
        window_start = current_time - self.window_size_ms
        while self.window and self.window[0] < window_start:
            self.window.popleft()
        if len(self.window) < self.capacity:
            self.window.append(current_time)
            return True
        return False


class SlidingWindowRateLimiting(RateLimitingStrategy):
    def __init__(self, capacity: int, window_size_ms: int):
        self.capacity = capacity
        self.window_size_ms = window_size_ms
        self.windows: Dict[str, SlidingWindow] = {}
        self.lock = threading.Lock()

    def allowed(self, request: Request) -> bool:
        with self.lock:
            window = self.windows.setdefault(
                request.user_id, SlidingWindow(self.capacity, self.window_size_ms)
            )
            return window.is_allowed()


def fire_burst(strategy: RateLimitingStrategy, label: str, user_id: str, count: int) -> None:
    print(f"{label} -- firing {count} back-to-back requests for {user_id}:")
    for i in range(1, count + 1):
        ok = strategy.allowed(Request(user_id, "10.0.0.1"))
        print(f"  request #{i} -> {'ALLOWED' if ok else 'REJECTED'}")


def main() -> None:
    # Capacity 3, refills 1 token/sec: the first 3 requests burst through,
    # the 4th is rejected immediately after.
    token_bucket = TokenBucketStrategy(capacity=3, refill_rate=1)
    fire_burst(token_bucket, "TokenBucket", "alice", 4)

    print("\nWaiting 1.1s for the token bucket to refill one token...")
    time.sleep(1.1)
    after_refill = token_bucket.allowed(Request("alice", "10.0.0.1"))
    print(f"request after refill -> {'ALLOWED' if after_refill else 'REJECTED'}")

    print()
    # A second, independent user must not be affected by alice's usage.
    fire_burst(token_bucket, "TokenBucket", "bob", 2)

    print()
    # Capacity 3 requests per 500ms sliding window.
    sliding_window = SlidingWindowRateLimiting(capacity=3, window_size_ms=500)
    fire_burst(sliding_window, "SlidingWindow", "carol", 4)

    print("\nWaiting 600ms for carol's window to fully slide past...")
    time.sleep(0.6)
    after_slide = sliding_window.allowed(Request("carol", "10.0.0.1"))
    print(f"request after window slides -> {'ALLOWED' if after_slide else 'REJECTED'}")


if __name__ == "__main__":
    main()
