package Concurrency;

import java.util.LinkedList;
import java.util.Queue;

public class CustomThreadPoolDemo {

    static class CustomThreadPool {
        private final Queue<Runnable> taskQueue = new LinkedList<>();
        private final Worker[] workers;
        private final Object lock = new Object();
        private volatile boolean isShutdown = false;

        CustomThreadPool(int numThreads) {
            workers = new Worker[numThreads];
            for (int i = 0; i < numThreads; i++) {
                workers[i] = new Worker("Worker-" + i);
                workers[i].start();
            }
        }

        public void submit(Runnable task) {
            synchronized (lock) {
                if (isShutdown) {
                    throw new IllegalStateException("ThreadPool is shut down, cannot accept new tasks");
                }
                taskQueue.add(task);
                lock.notify();
            }
        }

        public void shutdown() {
            synchronized (lock) {
                isShutdown = true;
                lock.notifyAll();
            }
        }

        public void awaitTermination() throws InterruptedException {
            for (Worker worker : workers) {
                worker.join();
            }
        }

        private class Worker extends Thread {
            Worker(String name) {
                super(name);
            }

            @Override
            public void run() {
                while (true) {
                    Runnable task;
                    synchronized (lock) {
                        while (taskQueue.isEmpty() && !isShutdown) {
                            try {
                                lock.wait(); //worker need to wait
                            } catch (InterruptedException e) {
                                Thread.currentThread().interrupt();
                                return;
                            }
                        }
                        if (taskQueue.isEmpty() && isShutdown) {
                            return; // no more work, pool is shutting down
                        }

                        // worker will pull the task and process the task
                        task = taskQueue.poll();
                    }
                    try {
                        task.run();
                    } catch (RuntimeException e) {
                        System.out.println(getName() + " task threw exception: " + e.getMessage());
                    }
                }
            }
        }
    }

    public static void main(String[] args) throws InterruptedException {
        CustomThreadPool pool = new CustomThreadPool(3);

        for (int i = 0; i < 10; i++) {
            final int taskId = i;
            pool.submit(() -> {
                System.out.println(Thread.currentThread().getName() + " executing task " + taskId);
                try {
                    Thread.sleep(200);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            });
        }

        pool.shutdown();
        pool.awaitTermination();
        System.out.println("All tasks completed, pool terminated.");
    }
}