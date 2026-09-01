

interface IRateLimitingStrategy{
    boolean isAllowed(Request request);
}

class TokenBucketStrategy implements IRateLimitingStrategy{

    @Override
    public boolean isAllowed(Request request) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'isAllowed'");
    }
    
}

class SlidingWindowStrategy implements IRateLimitingStrategy{

    @Override
    public boolean isAllowed(Request request) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'isAllowed'");
    }
    
}



class NotificationService{
    IRateLimitingStrategy strategy;
    
    public void setStrategy(IRateLimitingStrategy strategy){
        this.strategy = strategy;
    }

}
