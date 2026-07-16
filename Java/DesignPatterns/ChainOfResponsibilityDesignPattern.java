package DesignPatterns;

class Request{
    int priority;
}

abstract class IHandler{
    private IHandler nextHandler;

    IHandler setNextHandler(IHandler handler){
        this.nextHandler = handler;
        return this.nextHandler;
    }
    IHandler getNextHandler(){
        return nextHandler;
    }
    abstract boolean isAllowed(Request request);
    abstract void processRequest(Request request);
}

class Level1Handler extends IHandler{

    @Override
    boolean isAllowed(Request request) {
        if(request.priority < 8)
            return true;
        else
            return false;
    }

    @Override
    void processRequest(Request request) {
        if(isAllowed(request)){
            System.out.println("Processing the request : lvel1 is processing");
        }else{
            getNextHandler().processRequest(request);
        }
    }

}

class Level2Handler extends IHandler{

    @Override
    boolean isAllowed(Request request) {
        if(request.priority < 15)
            return true;
        else
            return false;
    }

    @Override
    void processRequest(Request request) {
        if(isAllowed(request)){
            System.out.println("Processing the request : lvel1 is processing");
        }else{
            getNextHandler().processRequest(request);
        }
       
    }

}

class Level3Handler extends IHandler{

    @Override
    boolean isAllowed(Request request) {
        if(request.priority < 2)
            return true;
        else
            return false;
    }
    void processRequest(Request request) {
        if(isAllowed(request)){
            System.out.println("Processing the request : lvel1 is processing");
        }else{
            getNextHandler().processRequest(request);
        }
    }
}

public class ChainOfResponsibilityDesignPattern {
    public static void main(String[] args) {
        Level1Handler level1 = new Level1Handler();
        Level2Handler level2 = new Level2Handler();
        Level3Handler level3 = new Level3Handler();

        // level1.setNextHandler(level2);
        // level2.setNextHandler(level3);
        
        level1.setNextHandler(level2).setNextHandler(level3);


    }
}
