package Interview_problems;


// NotificationSystem
/*

1. Understand the problem, ask clarifying questions 

2. List down the functional and non functional requirements

functional requirements:
1.
2.
3.
4.
5.

Non-functional requirements:
1.
2.
3.
4.

3. Identifying the core entities and Relationship

Identifying the Core Entity?
Classes, Interfaces, Enums

Notification
NotificationChannel
NotificationTypes
ConcreateNotificationChannels{SMS, EMAIL}

NotificationService
NotificationDispatcher

*/
enum NotificationType{
    SMS, EMAIL, SLACK, WHATSAPP
}

class Notification{
    NotificationType type;
    String id;
    String message;
}

interface NotificationChannel{
    void sendNotification(Notification notification);
}

class SMSNotification implements NotificationChannel{
    @Override
    public void sendNotification(Notification notification) {

    }
}


class EmailNotification implements NotificationChannel{
    @Override
    public void sendNotification(Notification notification) {

    }
}

class SlackNotification implements NotificationChannel{
    @Override
    public void sendNotification(Notification notification) {

    }
}


public class LLD_questions {
    
}



/*
    Posting Questions 
        --> go over internet... understand... functional non 
        Questions 

        2 Questions Everyday 

*/

// Next Class: 