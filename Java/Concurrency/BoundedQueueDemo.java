package Concurrency;

import java.util.concurrent.*;
import java.util.LinkedList;
import java.util.Queue;

public class BoundedQueueDemo {

    static class BoundedQueue<T> {
        private final Queue<T> queue = new LinkedList<>();
        private final int capacity;

        public BoundedQueue(int capacity) {
            this.capacity = capacity;
        }

        public synchronized void put(T item) throws InterruptedException {
            while (queue.size() == capacity) {          // full — wait for a taker
                System.out.println(Thread.currentThread().getName() + " sees FULL (" + queue.size() + "/" + capacity + "), waiting...");
                wait();
            }
            queue.offer(item);
            System.out.println(Thread.currentThread().getName() + " put: " + item + " [size=" + queue.size() + "]");
            notifyAll();                                  // wake possible waiting takers (and putters — see note below)
        }

        public synchronized T take() throws InterruptedException {
            while (queue.isEmpty()) {                     // empty — wait for a putter
                System.out.println(Thread.currentThread().getName() + " sees EMPTY, waiting...");
                wait();
            }
            T item = queue.poll();
            System.out.println(Thread.currentThread().getName() + " took: " + item + " [size=" + queue.size() + "]");
            notifyAll();
            return item;
        }
    }

    public static void main(String[] args) throws InterruptedException {
        BoundedQueue<String> queue = new BoundedQueue<>(3);   // capacity 3, not 1
        ExecutorService pool = Executors.newFixedThreadPool(4);

        Runnable producer = () -> {
            try {
                for (int i = 0; i < 5; i++) {
                    queue.put(Thread.currentThread().getName() + "-item" + i);
                    Thread.sleep(30);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        };

        Runnable consumer = () -> {
            try {
                for (int i = 0; i < 5; i++) {
                    queue.take();
                    Thread.sleep(60);
                }
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        };

        pool.submit(producer);
        pool.submit(producer);
        pool.submit(consumer);
        pool.submit(consumer);

        pool.shutdown();
        pool.awaitTermination(5, TimeUnit.SECONDS);
    }
}