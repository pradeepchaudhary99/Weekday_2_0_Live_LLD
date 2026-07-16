package Concurrency;
import java.util.concurrent.locks.*;
import java.util.concurrent.*;

public class ReadWriteLockDemo {

    static class SharedCounter {
        private int value = 0;
        private final ReadWriteLock rwLock = new ReentrantReadWriteLock();

        public int read() {
            rwLock.readLock().lock();
            try {
                System.out.println(Thread.currentThread().getName() + " reading: " + value);
                sleep(100);  // simulate read taking some time
                return value;
            } finally {
                rwLock.readLock().unlock();
            }
        }

        public void write(int newValue) {
            rwLock.writeLock().lock();
            try {
                System.out.println(Thread.currentThread().getName() + " WRITING: " + newValue);
                sleep(100);
                value = newValue;
            } finally {
                rwLock.writeLock().unlock();
            }
        }

        private void sleep(long ms) {
            try { Thread.sleep(ms); } catch (InterruptedException e) { Thread.currentThread().interrupt(); }
        }
    }

    public static void main(String[] args) throws InterruptedException {
        SharedCounter counter = new SharedCounter();
        ExecutorService pool = Executors.newFixedThreadPool(5);

        // 4 readers — watch their timestamps overlap
        for (int i = 0; i < 4; i++) {
            pool.submit(counter::read);
        }

        Thread.sleep(20); // let readers start first

        // 1 writer — watch it wait for ALL readers to finish, then block everyone else
        pool.submit(() -> counter.write(99));

        pool.shutdown();
        pool.awaitTermination(5, TimeUnit.SECONDS);
    }
}