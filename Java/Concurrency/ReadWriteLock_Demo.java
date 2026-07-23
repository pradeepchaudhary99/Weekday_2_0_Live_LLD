

public class ReadWriteLock_Demo{
    int activeReaders = 0;
    boolean activeWriter = false; // if only 1 writer is allowed, other int 
    int waitingWriters = 0;

    public synchronized void lockRead(){
        //BLOCK ANY read locks if there is already a write lock 
        while(activeWriter || waitingWriters > 0){
            wait();
        }
        activeReaders++;
    }

    public synchronized void unlockRead (){
        activeReaders--;
        if(activeReaders == 0){
            notifyAll();
        }
    }

    public synchronized void lockWrite() throws InterruptedException{
        waitingWriters++; // 500
        try{
            while(activeWriter || activeReaders > 0){
                wait();
            }
        }finally{
            waitingWriters--;
        }
        activeWriter = true;
    }

    public synchronized void unlockWrite(){
        activeWriter = false;
        notifyAll();
    }

}