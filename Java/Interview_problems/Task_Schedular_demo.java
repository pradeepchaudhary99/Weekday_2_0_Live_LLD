

/*

FR: 
    Schedule a task for a future time 
    Execute task at a scheduled time 
    Support task priority 
    Support recurring task 
    cancel a scheduled task 
    Thread safe scheduling 
    Task should be executed async

*/

interface Task{
    void execute();
}

class EmailTask implements Task{
    String email;

    @Override
    public void execute() {

    }    
}

class PaymentTask implements Task{

    @Override
    public void execute() {

    }
    
}

class ScheduledTask{
    String id;
    Task task;
    long executionTime;
    int priority;
}

class TaskScheduler{
    TaskQueue taskQueue;

    void schedule(ScheduledTask task){
        taskQueue.add(task);
    }

}

class TaskQueue{
    PriorityQueue<SceduledTask> queue;


    void add(ScheduledTask task){
        queue.offer(task);
    }
}

class Dispatcher implements Runnable{
    TaskQueue taskQueue;
    boolean isRunning = true;
    ExecutorService workerPool;
    @Override
    public void run() {

        while(running){
            task = taskQueue.peek();


            // what is the current time 
            // delay = 




            
        }

    }


    
}



public class Task_Schedular_demo {
    
}
