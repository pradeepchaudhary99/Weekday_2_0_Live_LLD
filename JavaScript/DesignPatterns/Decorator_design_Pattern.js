class Notification {
    send(message) {
        throw new Error("Not implemented");
    }
}

class SMSNotification extends Notification {
    send(message) {
        console.log("SMS is Sent: message" + message);
    }
}

class Whatsapp extends Notification {
    send(message) {
        console.log("whatsapp is Sent: message" + message);
    }
}

class Decorator extends Notification {
    constructor(wrapped_object) {
        super();
        this.wrapped_object = wrapped_object;
    }
}

class RateLimiter extends Decorator {
    constructor(wrapped_object) {
        super(wrapped_object);
    }

    send(message) {
        console.log("Logic of Rate limiting 1000 Lines");
        console.log("Logic of Rate limiting 1000 Lines");
        console.log("Logic of Rate limiting 1000 Lines");
        console.log("Logic of Rate limiting 1000 Lines");
        console.log("Logic of Rate limiting 1000 Lines");
        this.wrapped_object.send(message);
    }
}

class LoadBalancer extends Decorator {
    constructor(wrapped_object) {
        super(wrapped_object);
    }

    send(message) {
        console.log("Logic Load balancer 1000 Lines");
        this.wrapped_object.send(message);
    }
}

class JSonFormatter extends Decorator {
    constructor(wrapped_object) {
        super(wrapped_object);
    }

    send(message) {
        console.log("Logic Json 1000 Lines");
        this.wrapped_object.send(message);
    }
}

class WhatsappFormtter extends Decorator {
    constructor(wrapped_object) {
        super(wrapped_object);
    }

    send(message) {
        console.log("whatsapp formetter");
        this.wrapped_object.send(message);
    }
}

class Decorator_design_Pattern {
    static main(args) {
        // Notification notification1 = new JSonFormatter(new RateLimiter(new LoadBalancer(new SMSNotification())));
        // notification1.send("pradeep");
        const notification2 = new WhatsappFormtter(new WhatsappFormtter(new RateLimiter((new Whatsapp()))));
        notification2.send("pradeep");
    }
}

Decorator_design_Pattern.main([]);
