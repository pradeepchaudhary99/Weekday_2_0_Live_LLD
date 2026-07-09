class Notification {
    send(message) {
        throw new Error("Not implemented");
    }
}

class NotificationSMS extends Notification {
    send(message) {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'send'");
    }
}

class NotificationSlack extends Notification {
    send(message) {
        // TODO Auto-generated method stub
        throw new Error("Unimplemented method 'send'");
    }
}

class NotificationService_without_factory {
    sendNotification(type, message) {
        if (type === "SMS") {
            new NotificationSMS().send(message);
        } else if (type === "Slack") {
            new NotificationSlack().send(message);
        }
    }
}

class NotificationService_with_factory {
    sendNotification(type, message) {
        const notification = NotificationFactory.getInstance(type, message);
        notification.send(message);
    }
}

// Simple Factory
class NotificationFactory {
    static getInstance(type, message) {
        if (type === "SMS") {
            return new NotificationSMS();
        } else if (type === "Slack") {
            return new NotificationSlack();
        } else {
            return new NotificationSMS();
        }
    }
}

class Factory_design_pattern {

}
