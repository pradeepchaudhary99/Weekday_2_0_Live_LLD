import java.util.HashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.Queue;

interface RateLimitingStrategy{
    boolean allowed(Request request);
}


class TokenBucket{
    int capacity;
    int tokens;
    long lastRefillTime;
    int refillRate; 

    boolean isAllowed(){

        refill();

        if(tokens > 0){
            tokens--;
            return true;
        }else{
            return false;
        }
    }
    void refill(){
        long currentTime = System.currentTimeMillis();
        long timeDiff = (currentTime - lastRefillTime)/1000;
        int claimedTokens = (int)timeDiff * refillRate;
        tokens = Math.min(capacity, tokens +  claimedTokens);
        lastRefillTime = currentTime;
    }

}

class Request{
    String userId;
    String ip;
}



//Akash, Nishaanth
class TokenBucketStrategy implements RateLimitingStrategy{
    Map<String, TokenBucket> tokenBuckets = new HashMap<>();

    @Override
    public boolean allowed(Request request) {
        TokenBucket bucket = tokenBuckets.get(request.userId);

        if(bucket.isAllowed()){
            //its allowed do the work 
            return true;
        }else{
            // error codes 
            return false;
        }
    }



    
}


class SlidingWindow{
    int capacity;
    Queue<Long> window = new LinkedList<>();
    Long windowSize;
    boolean isAllowed(){
        Long currentTime = System.currentTimeMillis();
        Long startWindow = currentTime - windowSize;
        while(window.size() > 0 && startWindow > window.peek()){
            window.poll();
        }
        if(window.size() < capacity){
            window.add(currentTime);
            return true;
        }else{
            return false;
        }
    }
}


class SlidingWindowRateLimiting implements RateLimitingStrategy{


    
    @Override
    public boolean allowed(Request request) {

    }
    
}
