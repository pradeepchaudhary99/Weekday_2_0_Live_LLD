package Java.DesignPatterns;


interface Notification{
    void send(String message);
}
class NotificationSMS implements Notification{

    @Override
    public void send(String message) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'send'");
    }

}

class NotificationSlack implements Notification{

    @Override
    public void send(String message) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'send'");
    }

}

class NotificationService_without_factory{

    void sendNotification(String type, String message){
        if(type.equals("SMS")){
            new NotificationSMS().send(message);
        }
        else if(type.equals("Slack")){
            new NotificationSlack().send(message);
        }
    }
}

class NotificationService_with_factory{
   
    void sendNotification(String type, String message){
        Notification notification = NotificationFactory.getInstance(type, message);
        notification.send(message);
    }
}

// Simple Factory 
class NotificationFactory{
    static Notification getInstance(String type, String message){
        if(type.equals("SMS")){
            return new NotificationSMS();
        }
        else if(type.equals("Slack")){
            return new NotificationSlack();
        }else{
            return new NotificationSMS();
        }
    }
}

public class Factory_design_pattern {
    
}
