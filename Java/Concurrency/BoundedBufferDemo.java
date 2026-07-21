import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

public class BoundedBufferDemo {

    static class BoundedBuffer<T> {
        private final Queue<T> queue = new LinkedList<>();
        private final int capacity;
        private final ReentrantLock lock = new ReentrantLock();
        private final Condition producer = lock.newCondition();
        private final Condition consumer = lock.newCondition();

        BoundedBuffer(int capacity) {
            this.capacity = capacity;
        }

        void put(T item) throws InterruptedException {
           
           //t1, t2, t3, t4
            lock.lock();
            try {
                while (queue.size() == capacity) {
                    producer.await();
                }
                queue.add(item);
                consumer.signal();
            } finally {
                lock.unlock();
            }
        }

        T take() throws InterruptedException {
            lock.lock();
            try {
                while (queue.isEmpty()) {
                    consumer.await();
                }
                T item = queue.poll();
                producer.signal();
                return item;
            } finally {
                lock.unlock();
            }
        }
    }

    public static void main(String[] args) throws InterruptedException {
        BoundedBuffer<Integer> buffer = new BoundedBuffer<>(5);

        Runnable producer = () -> {
            for (int i = 0; i < 10; i++) {
                try {
                    buffer.put(i);
                    System.out.println("Produced: " + i);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        };

        Runnable consumer = () -> {
            for (int i = 0; i < 10; i++) {
                try {
                    int item = buffer.take();
                    System.out.println("Consumed: " + item);
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }
        };

        Thread t1 = new Thread(producer);
        Thread t2 = new Thread(consumer);
        t1.start();
        t2.start();
        t1.join();
        t2.join();
    }
}