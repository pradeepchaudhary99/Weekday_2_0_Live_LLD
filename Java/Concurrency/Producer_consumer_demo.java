import java.util.LinkedList;
import java.util.Queue;

class BoundedBuffer<T> {
    private final Queue<T> queue = new LinkedList<>();
    private final int capacity;

    BoundedBuffer(int capacity) {
        this.capacity = capacity;
    }

 
    synchronized void put(T item) throws InterruptedException {
        while (queue.size() == capacity) {
            wait();
        }
        queue.add(item);

        notifyAll();
    }

    synchronized T take() throws InterruptedException {
        while (queue.isEmpty()) {
            wait();
        }

        T item = queue.poll();
        notifyAll();
        return item;
    }
}

public class Producer_consumer_demo {
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