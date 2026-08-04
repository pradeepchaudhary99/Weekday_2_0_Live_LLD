package Concurrency;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

public class RaceDemo {
    static int unsafeCounter = 0;
    static AtomicInteger safeCounter = new AtomicInteger(0);

    public static void main(String[] args) throws InterruptedException {
        ExecutorService pool = Executors.newFixedThreadPool(8);
        for (int i = 0; i < 1000; i++) {
            pool.submit(
                () -> {
                unsafeCounter++;              // NOT atomic — three steps
                safeCounter.incrementAndGet(); // atomic
            });
        }
        pool.shutdown();
        pool.awaitTermination(5, TimeUnit.SECONDS);
        System.out.println("unsafe: " + unsafeCounter);  // almost never 1000
        System.out.println("safe:   " + safeCounter.get()); // always 1000
    }
}
