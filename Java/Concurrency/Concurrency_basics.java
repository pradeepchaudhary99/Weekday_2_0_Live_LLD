import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;


class Pradeep implements Runnable{

    @Override
    public void run() {
        while(true){
            System.out.println("Pradeep is continously working");
            System.out.println("Pradeep Running this in a new Thread: " + Thread.currentThread().getName());
        }
    }
}

public class Concurrency_basics{

        public static void main(String[] args) {
            
            Pradeep p1 = new Pradeep();
            Pradeep p2 = new Pradeep();

            Thread t1 = new Thread(p1);
            Thread t2 = new Thread(p2);
            
            t1.start();
            t2.start();



            // ExecutorService pool = Executors.newFixedThreadPool(5); ///threadPool
            // Runnable task1 = ()->{
            //     for(int i = 0; i < 20; i++)
            //     System.out.println("pradeep");
            // };
            
            // pool.submit(task1);
            // pool.shutdown();
    /* 
        Runnable task1 = ()->{
            for(int i = 0; i < 20; i++)
            System.out.println("pradeep");
        };

        Runnable task2 = ()->{
            for(int i = 0; i < 20; i++)
            System.out.println("ishita");
        };

        Runnable task3 = ()->{
            for(int i = 0; i < 20; i++)
            System.out.println("Sahil");
        };

        Runnable [] tasks = new Runnable[3];
        tasks[0] = task1;
        tasks[1] = task2;
        tasks[2] = task3;

        List<Thread> threads = new ArrayList<>();
        
        for(Runnable task : tasks){
              Thread t1 = new Thread(task);
              threads.add(t1);  
              t1.start();
        }

        try {
            for(Thread t : threads){
                t.join();
            }
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        System.out.println("main thread");
    
    
        */
    }
  




}