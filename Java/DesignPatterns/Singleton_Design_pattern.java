
// package Java.DesignPatterns;

class Logger{
    String filePath; 
    static Logger instance = null;
    private Logger(String filePath){
        System.out.println("object Created");
        this.filePath = filePath;
    }
    public static Logger getInstance(){

        System.out.println("Thread");
        if(instance == null)
            instance = new Logger("filePath");
        return instance;
    
    
    }
}


public class Singleton_Design_pattern {
    public static void main(String[] args) {
        Logger logger = Logger.getInstance();
        
        Runnable task = ()->{
            Logger.getInstance();
        };

  
        Thread thread1 = new Thread(task);
        Thread thread2 = new Thread(task);
        Thread thread3 = new Thread(task);
        
        thread1.start();
        thread2.start();
        thread3.start();
        

    

    }
}
