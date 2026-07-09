

interface Notification{
    void send(String message);
}

class SMSNotification implements Notification{
    @Override
    public void send(String message) {
        System.out.println("SMS is Sent: message" +message);
    }
}

class Whatsapp implements Notification{
    @Override
    public void send(String message) {
        System.out.println("whatsapp is Sent: message" +message);
    }
}

abstract class Decorator implements Notification{
    protected Notification wrapped_object;
    public Decorator(Notification wrapped_object){
        this.wrapped_object = wrapped_object;
    }
}

class RateLimiter extends Decorator{
    public RateLimiter(Notification wrapped_object) {
            super(wrapped_object);
    }
    
    @Override
    public void send(String message) {
        System.out.println("Logic of Rate limiting 1000 Lines");
        System.out.println("Logic of Rate limiting 1000 Lines");
        System.out.println("Logic of Rate limiting 1000 Lines");
        System.out.println("Logic of Rate limiting 1000 Lines");
        System.out.println("Logic of Rate limiting 1000 Lines");
        wrapped_object.send(message);
    }
}

class LoadBalancer extends Decorator{
    public LoadBalancer(Notification wrapped_object) {
        super(wrapped_object);
}

@Override
public void send(String message) {
        System.out.println("Logic Load balancer 1000 Lines");
        wrapped_object.send(message);
    }
}

class JSonFormatter extends Decorator{
    public JSonFormatter(Notification wrapped_object) {
        super(wrapped_object);
    }

    @Override
    public void send(String message) {
        System.out.println("Logic Json 1000 Lines");
        wrapped_object.send(message);
    }
}

class WhatsappFormtter extends Decorator{
    public WhatsappFormtter(Notification wrapped_object) {
        super(wrapped_object);
    }

    @Override
    public void send(String message) {
        System.out.println("whatsapp formetter");
        wrapped_object.send(message);
    }
}

public class Decorator_design_Pattern {
    public static void main(String[] args) {
        // Notification notification1 = new JSonFormatter(new RateLimiter(new LoadBalancer(new SMSNotification())));
        // notification1.send("pradeep"); 
        Notification notification2 = new WhatsappFormtter(new WhatsappFormtter(new RateLimiter((new Whatsapp()))));
        notification2.send("pradeep");          
    }    
}
