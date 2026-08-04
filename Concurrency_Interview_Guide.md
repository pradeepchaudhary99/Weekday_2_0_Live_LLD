# Threads & Concurrency Guide for LLD Interviews

A single-file reference covering everything you typically need for Low-Level Design (LLD) interviews involving multi-threading — concepts, patterns, pitfalls, and ready-to-adapt code sketches (Java, with notes for other languages).

---

## 1. Core Fundamentals

### 1.1 Process vs Thread
- **Process**: independent memory space, own resources. Heavyweight, isolated.
- **Thread**: lightweight unit of execution within a process. Threads of the same process **share heap memory** (objects, static fields) but each has its **own stack, program counter, and registers**.
- Implication for LLD: shared mutable state accessed by multiple threads is where all concurrency bugs come from.

### 1.2 Why Concurrency Bugs Happen
Three root causes, memorize these — interviewers love asking "what could go wrong here?":
1. **Race Condition** — outcome depends on thread scheduling order (e.g., two threads incrementing a counter without synchronization).
2. **Visibility Problem** — one thread's write to a variable isn't seen by another thread due to CPU caching/compiler reordering (fixed by `volatile`, synchronization, memory barriers).
3. **Ordering Problem** — instructions get reordered by compiler/CPU for optimization, breaking assumptions about happens-before relationships.

### 1.3 Java Memory Model (JMM) Essentials
- **`volatile`**: guarantees visibility (no caching in registers/CPU cache) and prevents instruction reordering around it. Does **NOT** guarantee atomicity (e.g., `count++` on a volatile int is still not thread-safe).
- **`synchronized`**: guarantees both **mutual exclusion** (only one thread in the critical section) and **visibility** (happens-before on lock acquire/release).
- **happens-before**: a guarantee that memory writes by one statement are visible to another statement, if the two statements are ordered by this relationship (lock unlock→lock, volatile write→read, thread start, thread join).

### 1.4 Creating & Starting Threads

Java gives you two classic ways to define "what a thread runs," plus the modern pool-based way. Know all three and when to reach for each.

**a) Implement `Runnable` (preferred)** — decouples the task from the threading mechanism, so the same task can be run on a plain `Thread`, in a pool, or scheduled later.

```java
class PrintTask implements Runnable {
    private final String message;
    PrintTask(String message) { this.message = message; }

    @Override
    public void run() {
        System.out.println(Thread.currentThread().getName() + ": " + message);
    }
}

Thread t = new Thread(new PrintTask("hello"));
t.start();     // starts a new OS thread and calls run() on it — NEVER call run() directly, that just runs it on the current thread
t.join();      // blocks the calling thread until t finishes
```

Lambda form (since `Runnable` is a functional interface — `void run()`, no args, no return):
```java
Thread t = new Thread(() -> System.out.println("running on " + Thread.currentThread().getName()));
t.start();
```

**b) Extend `Thread`** — simpler for quick demos, but ties your task to the threading mechanism (and Java has single inheritance, so you burn your one `extends` on it). Prefer `Runnable` unless you specifically need to override other `Thread` methods.

```java
class WorkerThread extends Thread {
    @Override
    public void run() {
        System.out.println("working on " + getName());
    }
}

WorkerThread w = new WorkerThread();
w.start();
```

**c) `Callable<V>` — like `Runnable` but returns a value and can throw checked exceptions.** Only usable via an `ExecutorService` (no `Callable` constructor on `Thread`).

```java
Callable<Integer> task = () -> {
    Thread.sleep(100);
    return 42;
};

ExecutorService pool = Executors.newSingleThreadExecutor();
Future<Integer> future = pool.submit(task);
Integer result = future.get();   // blocks until the task completes, rethrows task's exception wrapped in ExecutionException
pool.shutdown();
```

**`start()` vs `run()`** — the single most common beginner mistake: calling `t.run()` executes the code synchronously on the *current* thread (no new thread is created at all); only `t.start()` spins up a new thread and schedules `run()` to execute on it.

**Key `Thread` / lifecycle APIs to know:**
- `t.start()` — begin execution on a new thread (can only be called once per `Thread` object).
- `t.join()` / `t.join(millis)` — caller blocks until `t` finishes (or timeout elapses).
- `t.interrupt()` — requests cooperative cancellation; sets the interrupt flag, and if `t` is blocked in `sleep`/`wait`/`join`, throws `InterruptedException` there.
- `Thread.currentThread()` — reference to the thread executing the current code.
- `t.setDaemon(true)` — marks it as a background thread that won't stop the JVM from exiting (must be set before `start()`).
- Thread states: `NEW → RUNNABLE → (BLOCKED/WAITING/TIMED_WAITING) → TERMINATED`.

**In an LLD interview, default to submitting `Runnable`/`Callable` tasks to an `ExecutorService`** (see §3) rather than manually creating raw `Thread` objects — it shows you understand resource management (pooling, backpressure, lifecycle) instead of just "make it work."

---

## 2. Synchronization Primitives

### 2.1 Locks

| Primitive | Use case | Notes |
|---|---|---|
| `synchronized` (intrinsic lock / monitor) | Simple mutual exclusion | Reentrant, released automatically on exception, cannot try-lock or interrupt |
| `ReentrantLock` (`java.util.concurrent.locks`) | Need tryLock, timed lock, interruptible lock, fairness | Must manually `unlock()` in `finally` |
| `ReentrantReadWriteLock` | Many readers, few writers | Readers block writers and vice versa, but readers don't block readers |
| `StampedLock` | High-perf read-heavy workloads | Supports optimistic reads (Java 8+) |

```java
private final ReentrantLock lock = new ReentrantLock();

void criticalSection() {
    lock.lock();
    try {
        // shared state mutation
    } finally {
        lock.unlock();
    }
}
```

### 2.2 Reentrancy
A thread holding a lock can re-acquire it without deadlocking itself. Both `synchronized` and `ReentrantLock` are reentrant — important when a synchronized method calls another synchronized method on the same object.

### 2.3 Atomic Variables (Lock-free)
`java.util.concurrent.atomic`: `AtomicInteger`, `AtomicLong`, `AtomicReference`, `AtomicBoolean`.
- Built on **CAS (Compare-And-Swap)** — hardware-level atomic instruction.
- Use when you just need atomic increment/update of a single variable — avoids lock overhead.

```java
AtomicInteger counter = new AtomicInteger(0);
counter.incrementAndGet();          // atomic
counter.compareAndSet(expected, newValue);
```

**CAS mental model**: "If the current value equals what I last read, swap it for my new value; otherwise retry." This is the basis of most lock-free data structures — a favorite whiteboard topic.

### 2.4 Condition Variables
Used to make a thread wait until some condition becomes true, without busy-waiting.

- **Intrinsic**: `wait()`, `notify()`, `notifyAll()` — must be called inside a `synchronized` block on the same object monitor. Always call `wait()` in a `while` loop (not `if`) to guard against spurious wakeups.
- **Explicit**: `Condition` objects created from a `Lock` (`lock.newCondition()`), used with `await()`/`signal()`/`signalAll()`. Lets you have multiple wait-queues per lock (e.g., "not full" and "not empty" conditions on the same bounded buffer).

```java
class BoundedBuffer<T> {
    private final Queue<T> queue = new LinkedList<>();
    private final int capacity;
    private final ReentrantLock lock = new ReentrantLock();
    private final Condition notFull = lock.newCondition();
    private final Condition notEmpty = lock.newCondition();

    void put(T item) throws InterruptedException {
        lock.lock();
        try {
            while (queue.size() == capacity) notFull.await();
            queue.add(item);
            notEmpty.signal();
        } finally { lock.unlock(); }
    }

    T take() throws InterruptedException {
        lock.lock();
        try {
            while (queue.isEmpty()) notEmpty.await();
            T item = queue.poll();
            notFull.signal();
            return item;
        } finally { lock.unlock(); }
    }
}
```
This single snippet (Bounded Buffer / Producer-Consumer) is probably the **single most common LLD concurrency question**. Know it cold.

### 2.5 Semaphore
Controls access to a resource pool with a fixed number of permits.

```java
Semaphore semaphore = new Semaphore(3); // 3 permits, e.g., 3 parking spots
semaphore.acquire();
try {
    // use resource
} finally {
    semaphore.release();
}
```
Difference from a lock: a lock has exactly 1 permit and (usually) ownership semantics; a semaphore can have N permits and any thread can release.

### 2.6 CountDownLatch vs CyclicBarrier vs Phaser

| Tool | Purpose | Reusable? |
|---|---|---|
| `CountDownLatch` | One or more threads wait for N events to complete (e.g., wait for all workers to finish startup) | No — one-time use |
| `CyclicBarrier` | N threads wait for **each other** to reach a common point, then all proceed together | Yes — resets automatically |
| `Phaser` | Advanced, dynamic party count, multi-phase synchronization | Yes |

```java
CountDownLatch latch = new CountDownLatch(3);
// each worker thread calls latch.countDown() when done
latch.await(); // main thread blocks until count reaches 0
```

### 2.7 Exchanger
Two threads swap objects at a synchronization point. Rarely asked, but good to mention exists.

---

## 3. Thread Pools & Executors

### 3.1 Why Thread Pools
Creating a thread is expensive (OS-level resource). Thread pools reuse a fixed set of worker threads to run submitted tasks, avoiding creation/teardown overhead and bounding resource usage.

### 3.2 Java's `ExecutorService`

```java
ExecutorService pool = Executors.newFixedThreadPool(4);
Future<Integer> future = pool.submit(() -> computeSomething());
Integer result = future.get(); // blocks until done
pool.shutdown();
```

Common factory methods (know their trade-offs, don't just name them):
- `newFixedThreadPool(n)` — fixed size, unbounded queue → risk of OOM under heavy load.
- `newCachedThreadPool()` — unbounded threads, creates new ones as needed, reaps idle ones after 60s → risk of thread explosion.
- `newSingleThreadExecutor()` — serializes tasks, guarantees order.
- `newScheduledThreadPool(n)` — for delayed/periodic tasks.

### 3.3 ThreadPoolExecutor — the one you should be able to build from scratch
This is a **very common LLD ask**: "Design a thread pool."

```java
ThreadPoolExecutor executor = new ThreadPoolExecutor(
    corePoolSize,      // threads kept alive even when idle
    maxPoolSize,        // max threads allowed
    keepAliveTime, TimeUnit.SECONDS,  // idle timeout for extra threads
    new LinkedBlockingQueue<>(queueCapacity), // task queue
    new ThreadPoolExecutor.CallerRunsPolicy()  // rejection policy
);
```

**Execution order when a task is submitted**:
1. If fewer than `corePoolSize` threads running → start a new thread.
2. Else if queue has space → enqueue the task.
3. Else if fewer than `maxPoolSize` threads running → start a new (non-core) thread.
4. Else → apply the **RejectedExecutionHandler** (AbortPolicy throws, CallerRunsPolicy runs on caller's thread, DiscardPolicy silently drops, DiscardOldestPolicy evicts head of queue).

**Minimal hand-rolled version** (interviewers sometimes want you to build this, not just use `java.util.concurrent`):

```java
class SimpleThreadPool {
    private final BlockingQueue<Runnable> taskQueue;
    private final List<WorkerThread> workers = new ArrayList<>();
    private volatile boolean isStopped = false;

    SimpleThreadPool(int numThreads, int queueCapacity) {
        taskQueue = new LinkedBlockingQueue<>(queueCapacity);
        for (int i = 0; i < numThreads; i++) {
            WorkerThread worker = new WorkerThread(taskQueue);
            workers.add(worker);
            worker.start();
        }
    }

    void submit(Runnable task) {
        if (isStopped) throw new IllegalStateException("Pool stopped");
        taskQueue.offer(task);
    }

    void shutdown() {
        isStopped = true;
        workers.forEach(WorkerThread::stopWorker);
    }

    static class WorkerThread extends Thread {
        private final BlockingQueue<Runnable> queue;
        private volatile boolean running = true;

        WorkerThread(BlockingQueue<Runnable> queue) { this.queue = queue; }

        public void run() {
            while (running) {
                try {
                    Runnable task = queue.take(); // blocks until available
                    task.run();
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        }

        void stopWorker() { running = false; this.interrupt(); }
    }
}
```

### 3.4 Futures / CompletableFuture
- `Future<T>`: represents a pending result; `.get()` blocks.
- `CompletableFuture<T>`: composable, non-blocking chaining (`thenApply`, `thenCompose`, `allOf`, `exceptionally`). Common in async pipeline design questions.

```java
CompletableFuture<Integer> future = CompletableFuture
    .supplyAsync(() -> fetchFromDb())      // runs on ForkJoinPool.commonPool() by default
    .thenApply(result -> result * 2)        // transform, still async
    .exceptionally(ex -> -1);               // fallback on failure

Integer value = future.get();               // block only where you must
```
`supplyAsync` submits work to a pool immediately; `thenApply`/`thenCompose` chain callbacks without blocking the calling thread. Use `thenCompose` (not `thenApply`) when the callback itself returns a `CompletableFuture`, to avoid nesting (`Future<Future<T>>`).

---

## 4. Concurrent Collections

| Collection | Notes |
|---|---|
| `ConcurrentHashMap` | Segmented/bucket-level locking (Java 8+: CAS + synchronized on bin heads). No global lock. `null` keys/values disallowed. |
| `CopyOnWriteArrayList` | Every write copies the underlying array. Great for read-heavy, rarely-written lists (e.g., listener lists). |
| `BlockingQueue` (interface) | `ArrayBlockingQueue`, `LinkedBlockingQueue`, `PriorityBlockingQueue`, `SynchronousQueue` — foundation of producer-consumer designs and thread pools. |
| `ConcurrentSkipListMap` | Sorted, concurrent alternative to `TreeMap`. |

**SynchronousQueue** is worth remembering separately: it has **zero capacity** — every `put` must wait for a matching `take`. Used internally by `newCachedThreadPool`.

---

## 5. Classic LLD Concurrency Problems (with the trick to each)

| Problem | Core idea |
|---|---|
| **Producer-Consumer** | Bounded buffer + two condition variables (`notFull`, `notEmpty`) or a `BlockingQueue` |
| **Dining Philosophers** | Deadlock avoidance: break circular wait — e.g., odd philosopher picks left fork first, even picks right first; or use a resource hierarchy / waiter semaphore |
| **Readers-Writers** | `ReentrantReadWriteLock`, or manual counter + mutex; decide read-preferring vs write-preferring vs fair |
| **Rate Limiter** | Token bucket / sliding window with `AtomicLong` + scheduled refill thread, or `Semaphore` with scheduled permit release |
| **Print odd/even numbers alternately with 2 threads** | Two threads, one lock, `wait/notify` ping-pong, or `Semaphore` pair |
| **Bounded Blocking Queue from scratch** | `ReentrantLock` + 2 `Condition`s (see 2.4) |
| **Thread-safe Singleton** | Double-checked locking with `volatile` instance, or eager init, or enum singleton, or holder class idiom |
| **Web Crawler / Parallel task processing** | `ExecutorService` + `ConcurrentHashMap` (visited set) + `CountDownLatch`/`CompletableFuture.allOf` |
| **Elevator / Traffic light / Parking lot (LLD + concurrency)** | Usually needs a lock around shared state mutation (assigning elevator, allocating parking spot) — identify the critical section, keep it small |
| **Uber/food-delivery matching** | Producer-consumer with `BlockingQueue` of ride requests, worker pool of drivers |

### 5.1 Thread-Safe Singleton (asked constantly)

```java
class Singleton {
    private static volatile Singleton instance; // volatile is essential

    private Singleton() {}

    public static Singleton getInstance() {
        if (instance == null) {                    // 1st check (no lock, fast path)
            synchronized (Singleton.class) {
                if (instance == null) {             // 2nd check (inside lock)
                    instance = new Singleton();
                }
            }
        }
        return instance;
    }
}
```
Why `volatile` matters here: without it, another thread could see a **partially constructed object** due to instruction reordering during `new Singleton()` (reference assigned before constructor finishes).

**Simpler & preferred in real interviews**: the *Initialization-on-demand holder idiom* — relies on JVM class-loading guarantees instead of double-checked locking:
```java
class Singleton {
    private Singleton() {}
    private static class Holder {
        static final Singleton INSTANCE = new Singleton();
    }
    public static Singleton getInstance() { return Holder.INSTANCE; }
}
```

### 5.2 Dining Philosophers (deadlock avoidance)

```java
class Philosopher extends Thread {
    private final Object leftFork, rightFork;
    private final int id;

    Philosopher(int id, Object leftFork, Object rightFork) {
        this.id = id;
        this.leftFork = leftFork;
        this.rightFork = rightFork;
    }

    public void run() {
        // Break circular wait: the last philosopher picks up right-then-left, everyone else left-then-right
        Object first = (id % 2 == 0) ? leftFork : rightFork;
        Object second = (id % 2 == 0) ? rightFork : leftFork;

        synchronized (first) {
            synchronized (second) {
                eat();
            }
        }
    }

    private void eat() { System.out.println("Philosopher " + id + " is eating"); }
}
```
Five philosophers, five forks, each philosopher needs both neighboring forks to eat. If everyone picks up their left fork first, you get circular wait → deadlock. The fix above breaks the cycle by making one philosopher (or every even-numbered one) reverse the pickup order, so at least one fork is always free somewhere in the ring. Alternative fixes: a `Semaphore(4)` that only lets 4 of 5 philosophers attempt to pick up forks at once, or a global lock ordering by fork ID.

### 5.3 Readers-Writers Lock

```java
class SharedCache<K, V> {
    private final Map<K, V> data = new HashMap<>();
    private final ReadWriteLock rwLock = new ReentrantReadWriteLock();
    private final Lock readLock = rwLock.readLock();
    private final Lock writeLock = rwLock.writeLock();

    V get(K key) {
        readLock.lock();
        try {
            return data.get(key);          // many readers can be in here concurrently
        } finally { readLock.unlock(); }
    }

    void put(K key, V value) {
        writeLock.lock();
        try {
            data.put(key, value);          // exclusive: blocks all readers and writers
        } finally { writeLock.unlock(); }
    }
}
```
`ReentrantReadWriteLock` lets any number of readers hold the read lock simultaneously, but a writer needs exclusive access — it blocks until all readers release, and blocks new readers while it holds the lock. Good fit whenever reads vastly outnumber writes (e.g., a config cache). Note: by default it's write-preferring-ish in fairness terms but the exact starvation behavior depends on the fairness mode you construct it with (`new ReentrantReadWriteLock(true)` for fair ordering).

### 5.4 Rate Limiter (Token Bucket)

```java
class TokenBucketRateLimiter {
    private final int capacity;
    private final AtomicInteger tokens;
    private final ScheduledExecutorService refiller = Executors.newScheduledThreadPool(1);

    TokenBucketRateLimiter(int capacity, int refillPerSecond) {
        this.capacity = capacity;
        this.tokens = new AtomicInteger(capacity);
        refiller.scheduleAtFixedRate(() ->
            tokens.updateAndGet(t -> Math.min(capacity, t + refillPerSecond)),
            1, 1, TimeUnit.SECONDS);
    }

    boolean tryAcquire() {
        int current;
        do {
            current = tokens.get();
            if (current <= 0) return false;        // no permit available, caller is rate-limited
        } while (!tokens.compareAndSet(current, current - 1));
        return true;
    }
}
```
A background thread refills tokens up to `capacity` once per second; each request does a lock-free CAS loop to grab a token. This is the standard "design a rate limiter" answer — know that other valid strategies are sliding-window-log, sliding-window-counter, and fixed-window-counter, each with different burst/accuracy trade-offs.

### 5.5 Print in Order with N Threads (generalized odd/even)

```java
class Printer {
    private int turn = 0;
    private final int n;
    private final Object lock = new Object();

    Printer(int n) { this.n = n; }

    void print(int threadId) {
        synchronized (lock) {
            while (turn % n != threadId) {
                try { lock.wait(); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
            }
            System.out.println("Thread " + threadId + " printed " + turn);
            turn++;
            lock.notifyAll();
        }
    }
}
```

---

## 6. Deadlock, Livelock, Starvation

- **Deadlock**: circular wait for resources — no thread can proceed. Requires all 4 Coffman conditions: mutual exclusion, hold-and-wait, no preemption, circular wait.
  - **Prevention**: always acquire locks in a **consistent global order**; use `tryLock()` with timeout; avoid nested locks where possible.
- **Livelock**: threads keep changing state in response to each other but make no progress (e.g., two people stepping aside for each other repeatedly).
- **Starvation**: a thread never gets CPU time/lock access because others are prioritized (e.g., unfair lock always favors certain threads).

**Deadlock example to be able to draw**:
```
Thread A: locks resource 1, waits for resource 2
Thread B: locks resource 2, waits for resource 1
```
Fix: enforce ordering — both threads must always lock resource 1 before resource 2.

**Runnable deadlock demo (and the fix):**

```java
Object lock1 = new Object();
Object lock2 = new Object();

// BUGGY — inconsistent lock order causes deadlock
Runnable threadA = () -> {
    synchronized (lock1) {
        synchronized (lock2) { /* work */ }
    }
};
Runnable threadB = () -> {
    synchronized (lock2) {                 // reversed order vs threadA
        synchronized (lock1) { /* work */ }
    }
};
// If A holds lock1 and B holds lock2 at the same instant, each blocks on the other's lock forever.

// FIX — both threads always acquire locks in the same global order (lock1 then lock2)
Runnable safeA = () -> {
    synchronized (lock1) {
        synchronized (lock2) { /* work */ }
    }
};
Runnable safeB = () -> {
    synchronized (lock1) {                 // same order as safeA
        synchronized (lock2) { /* work */ }
    }
};
```
The bug isn't "two locks" — it's that the two threads disagree on the *order* to acquire them. Whenever you must hold >1 lock, pick a canonical order (e.g., by object hash code, or by a fixed ID) and always acquire in that order.

---

## 7. Design Patterns Relevant to Concurrency

- **Thread-safe Singleton** (see 5.1).
- **Producer-Consumer** (see 2.4) — often modeled as its own mini design problem.
- **Monitor Object pattern** — encapsulate state + synchronization together (what `synchronized` methods effectively give you).
- **Active Object pattern** — decouples method invocation from execution; each object has its own thread/queue processing requests asynchronously (basis for actor-like systems).
- **Read-Write Lock pattern** — for caches with many reads, few writes.
- **Immutable Object pattern** — best concurrency fix is often "don't share mutable state." Immutable objects are inherently thread-safe (no synchronization needed).
- **Thread-per-request vs Worker pool** — tradeoff to discuss when designing a server (thread-per-request is simple but doesn't scale; worker pool bounds resource use).

---

## 8. Language Cheat-Sheet (if asked in a non-Java language)

- **Python**: GIL means only one thread executes Python bytecode at a time → use `threading` for I/O-bound work, `multiprocessing` for CPU-bound. `threading.Lock`, `Condition`, `Semaphore`, `Queue.Queue` (thread-safe) mirror Java's primitives conceptually.
- **C++**: `std::thread`, `std::mutex`, `std::lock_guard` / `std::unique_lock` (RAII locking), `std::condition_variable`, `std::atomic<T>`, `std::future`/`std::async`.
- **Go**: goroutines + channels (CSP model — "share memory by communicating, don't communicate by sharing memory"). `sync.Mutex`, `sync.WaitGroup`, `sync.Once`, buffered channels act like bounded queues, `select` for multiplexing.
- **JavaScript**: single-threaded event loop; "concurrency" is about async I/O (Promises, `async/await`), not true parallel threads (Node's `worker_threads` for actual parallelism).

The concepts (mutual exclusion, visibility, deadlock, producer-consumer, thread pools) transfer directly — only the syntax changes. In an interview, it's fine to explain the concept in Java-flavored pseudocode and note the equivalent primitive in the target language.

---

## 9. How to Approach a Concurrency LLD Question

1. **Identify shared mutable state** first — that's the only thing that needs protection.
2. **Minimize the critical section** — lock only what must be atomic; do expensive work (I/O, computation) outside the lock.
3. **Prefer higher-level utilities** (`ExecutorService`, `ConcurrentHashMap`, `BlockingQueue`, `CompletableFuture`) over hand-rolled `wait/notify` unless asked to build primitives from scratch.
4. **Say out loud what invariant you're protecting** — e.g., "I need atomicity between checking `size < capacity` and adding to the queue, so both must be inside the same lock."
5. **Call out failure modes**: what happens on exception inside a lock? (use `finally`), what happens on thread interruption? (propagate `InterruptedException` or restore interrupt status), what if a consumer thread dies? (supervise/restart).
6. **Discuss trade-offs**: fairness vs throughput, lock granularity (coarse = simple but slow, fine-grained = fast but deadlock-prone), blocking vs non-blocking (CAS/lock-free) design.
7. **Mention testing difficulty**: concurrency bugs are non-deterministic; mention stress testing, `jcstress`, thread sanitizers, or code review for lock ordering as ways to catch them.

---

## 10. Quick-Fire Q&A (rapid recall before an interview)

- **Q: Difference between `wait()` and `sleep()`?**
  `wait()` releases the monitor lock and must be called inside `synchronized`; `sleep()` does not release any lock and can be called anywhere.
- **Q: Why call `wait()` in a `while` loop, not `if`?**
  Spurious wakeups can occur; also another thread might grab the resource between `notify()` and this thread re-acquiring the lock.
- **Q: `notify()` vs `notifyAll()`?**
  `notify()` wakes one arbitrary waiting thread (risk: wrong thread wakes, or "lost wakeup"); `notifyAll()` wakes all, each re-checks its condition — safer default.
- **Q: Is `ArrayList` thread-safe?**
  No. Use `Collections.synchronizedList(...)`, `CopyOnWriteArrayList`, or external locking.
- **Q: What does `volatile` NOT do?**
  Doesn't make compound actions (`i++`) atomic, doesn't provide mutual exclusion.
- **Q: How does `ConcurrentHashMap` achieve high concurrency?**
  Fine-grained locking (bucket/bin-level) + CAS for common operations, no global lock, unlike a `synchronized` `HashMap` wrapper.
- **Q: Fixed vs Cached thread pool risk?**
  Fixed pool + unbounded queue → OOM if tasks pile up; cached pool → unbounded thread creation → resource exhaustion under load.
- **Q: How do you make an object immutable (and why does it help concurrency)?**
  Final fields, no setters, defensively copy mutable fields in/out, don't leak `this` during construction. Immutable objects need no synchronization since state never changes after construction.
- **Q: What's a race condition vs a data race?**
  Data race = two threads access the same memory concurrently, at least one write, no synchronization (a memory-model-level term). Race condition = a broader correctness bug where output depends on timing (can happen even with synchronization, e.g., check-then-act without atomicity).
