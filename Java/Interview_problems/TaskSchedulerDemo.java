import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicLong;

/*
FR:
    Schedule a task for a future time
    Execute task at a scheduled time
    Support task priority
    Support recurring task
    Cancel a scheduled task
    Thread safe scheduling
    Task should be executed async

Design notes:
    - ScheduledTask implements Delayed so a java.util.concurrent.DelayQueue can be used
      as the underlying queue. DelayQueue is thread-safe and natively blocks producers/
      consumers until a task's execution time has arrived - no manual wait/notify needed.
    - Ties (same executionTime) are broken by Priority (HIGH before LOW).
    - Dispatcher is a single background thread that blocks on queue.take() and hands the
      actual task execution off to a worker ExecutorService -> execution is async and the
      dispatcher thread is never blocked doing task work.
    - Cancellation is a soft-delete: a volatile `cancelled` flag is set, and we also try a
      best-effort DelayQueue.remove(). The dispatcher double-checks the flag before running.
    - Recurring tasks are re-inserted into the DelayQueue with a new executionTime after
      each run, so they perpetually flow back through the same pipeline.
*/

interface Task {
    void execute();
}

class EmailTask implements Task {
    private final String email;

    public EmailTask(String email) {
        this.email = email;
    }

    @Override
    public void execute() {
        System.out.println(Thread.currentThread().getName() + " -> sending email to " + email);
    }
}

class PaymentTask implements Task {
    private final String orderId;

    public PaymentTask(String orderId) {
        this.orderId = orderId;
    }

    @Override
    public void execute() {
        System.out.println(Thread.currentThread().getName() + " -> processing payment for order " + orderId);
    }
}

enum Priority {
    LOW, MEDIUM, HIGH
}

class ScheduledTask implements Delayed {
    private final String id;
    private final Task task;
    private volatile long executionTime; // epoch millis
    private final Priority priority;
    private final long recurringIntervalMillis; // 0 => one-shot
    private volatile boolean cancelled = false;

    ScheduledTask(String id, Task task, long executionTime, Priority priority, long recurringIntervalMillis) {
        this.id = id;
        this.task = task;
        this.executionTime = executionTime;
        this.priority = priority;
        this.recurringIntervalMillis = recurringIntervalMillis;
    }

    String getId() { return id; }
    Task getTask() { return task; }
    Priority getPriority() { return priority; }
    boolean isRecurring() { return recurringIntervalMillis > 0; }
    boolean isCancelled() { return cancelled; }
    void cancel() { cancelled = true; }

    void scheduleNextRun() {
        this.executionTime = System.currentTimeMillis() + recurringIntervalMillis;
    }

    @Override
    public long getDelay(TimeUnit unit) {
        long diffMillis = executionTime - System.currentTimeMillis();
        return unit.convert(diffMillis, TimeUnit.MILLISECONDS);
    }

    @Override
    public int compareTo(Delayed other) {
        if (other == this) return 0;
        long diff = getDelay(TimeUnit.MILLISECONDS) - other.getDelay(TimeUnit.MILLISECONDS);
        if (diff != 0) return diff < 0 ? -1 : 1;
        if (other instanceof ScheduledTask st) {
            // same execution time -> higher priority wins (HIGH before LOW)
            return st.priority.ordinal() - this.priority.ordinal();
        }
        return 0;
    }
}

class TaskQueue {
    private final DelayQueue<ScheduledTask> queue = new DelayQueue<>();
    private final ConcurrentMap<String, ScheduledTask> index = new ConcurrentHashMap<>();

    void add(ScheduledTask task) {
        index.put(task.getId(), task);
        queue.put(task);
    }

    ScheduledTask take() throws InterruptedException {
        return queue.take(); // blocks until earliest task's delay elapses
    }

    void forget(String id) {
        index.remove(id);
    }

    boolean cancel(String id) {
        ScheduledTask task = index.remove(id);
        if (task == null) return false;
        task.cancel();
        queue.remove(task); // best-effort; no-op if already dequeued by the dispatcher
        return true;
    }
}

class Dispatcher implements Runnable {
    private final TaskQueue taskQueue;
    private final ExecutorService workerPool;
    private volatile boolean running = true;

    Dispatcher(TaskQueue taskQueue, ExecutorService workerPool) {
        this.taskQueue = taskQueue;
        this.workerPool = workerPool;
    }

    void stop() {
        running = false;
    }

    @Override
    public void run() {
        while (running) {
            try {
                ScheduledTask scheduledTask = taskQueue.take();

                if (scheduledTask.isCancelled()) {
                    taskQueue.forget(scheduledTask.getId());
                    continue;
                }

                // hand off to worker pool -> async execution, dispatcher stays free
                workerPool.submit(() -> {
                    try {
                        scheduledTask.getTask().execute();
                    } catch (Exception e) {
                        System.err.println("Task " + scheduledTask.getId() + " failed: " + e.getMessage());
                    }
                });

                if (scheduledTask.isRecurring() && !scheduledTask.isCancelled()) {
                    scheduledTask.scheduleNextRun();
                    taskQueue.add(scheduledTask);
                } else {
                    taskQueue.forget(scheduledTask.getId());
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }
}

class TaskScheduler {
    private final TaskQueue taskQueue = new TaskQueue();
    private final ExecutorService workerPool;
    private final ExecutorService dispatcherThread = Executors.newSingleThreadExecutor();
    private final Dispatcher dispatcher;
    private final AtomicLong idGenerator = new AtomicLong();

    TaskScheduler(int workerPoolSize) {
        this.workerPool = Executors.newFixedThreadPool(workerPoolSize);
        this.dispatcher = new Dispatcher(taskQueue, workerPool);
        dispatcherThread.submit(dispatcher);
    }

    String schedule(Task task, long delayMillis, Priority priority) {
        return scheduleInternal(task, delayMillis, priority, 0);
    }

    String scheduleRecurring(Task task, long initialDelayMillis, long intervalMillis, Priority priority) {
        if (intervalMillis <= 0) {
            throw new IllegalArgumentException("intervalMillis must be > 0 for a recurring task");
        }
        return scheduleInternal(task, initialDelayMillis, priority, intervalMillis);
    }

    private String scheduleInternal(Task task, long delayMillis, Priority priority, long intervalMillis) {
        String id = "task-" + idGenerator.incrementAndGet();
        long executionTime = System.currentTimeMillis() + delayMillis;
        taskQueue.add(new ScheduledTask(id, task, executionTime, priority, intervalMillis));
        return id;
    }

    boolean cancel(String taskId) {
        return taskQueue.cancel(taskId);
    }

    void shutdown() {
        dispatcher.stop();
        dispatcherThread.shutdownNow();
        workerPool.shutdown();
    }
}

public class TaskSchedulerDemo {
    public static void main(String[] args) throws InterruptedException {
        TaskScheduler scheduler = new TaskScheduler(4);

        scheduler.schedule(new EmailTask("user@example.com"), 2000, Priority.HIGH);
        scheduler.schedule(new PaymentTask("ORDER-123"), 1000, Priority.MEDIUM);
        String recurringId = scheduler.scheduleRecurring(
                new EmailTask("digest@example.com"), 500, 1500, Priority.LOW);

        Thread.sleep(3000);
        boolean cancelled = scheduler.cancel(recurringId);
        System.out.println("Cancelled recurring task " + recurringId + ": " + cancelled);

        Thread.sleep(2000);
        scheduler.shutdown();
    }
}