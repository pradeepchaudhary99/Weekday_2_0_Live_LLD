/*

Functional Requirements:
    User should be able to send notifiation
    Notification system should support multiple types of channels
    notification system will support user preferences
    NS should process the notification asynchronous
    Retry failed notifications

Non-Functional Requirement:
    Error handling
    Asynchronous
    Atleast once delivery

*/


/*
Notification
NotificationService
NotificationDispather
NotificationChannel
    SMSNotificationChannel
    WhatsappNotificationChannel
    .....
NotificationFactory
User

*/

import java.util.*;
import java.util.concurrent.*;

enum NotificationType {
    SMS,
    WHATSAPP,
    EMAIL
}

enum NotificationStatus {
    PENDING,
    SENT,
    FAILED
}

class User {
    String id;
    String name;

    User(String id, String name) {
        this.id = id;
        this.name = name;
    }
}

class Notification {
    String id;
    User user;
    String recipientId;
    String message;
    int priority; // lower value = higher priority (e.g. TRANSACTION = 0)
    NotificationType type;
    NotificationStatus status;

    Notification(String id, User user, String recipientId, String message, int priority, NotificationType type) {
        this.id = id;
        this.user = user;
        this.recipientId = recipientId;
        this.message = message;
        this.priority = priority;
        this.type = type;
        this.status = NotificationStatus.PENDING;
    }
}

interface NotificationChannel {
    boolean sendNotification(Notification notification);
}

class SMSNotificationChannel implements NotificationChannel {
    @Override
    public boolean sendNotification(Notification notification) {
        System.out.println("[SMS] to " + notification.recipientId + ": " + notification.message);
        return true;
    }
}

class WhatsappNotificationChannel implements NotificationChannel {
    @Override
    public boolean sendNotification(Notification notification) {
        System.out.println("[WhatsApp] to " + notification.recipientId + ": " + notification.message);
        return true;
    }
}

class EmailNotificationChannel implements NotificationChannel {
    @Override
    public boolean sendNotification(Notification notification) {
        System.out.println("[Email] to " + notification.recipientId + ": " + notification.message);
        return true;
    }
}

class NotificationChannelFactory {
    private final Map<NotificationType, NotificationChannel> registry = new HashMap<>();

    NotificationChannelFactory() {
        registry.put(NotificationType.SMS, new SMSNotificationChannel());
        registry.put(NotificationType.WHATSAPP, new WhatsappNotificationChannel());
        registry.put(NotificationType.EMAIL, new EmailNotificationChannel());
    }

    NotificationChannel getNotificationChannel(NotificationType type) {
        NotificationChannel channel = registry.get(type);
        if (channel == null) {
            throw new IllegalArgumentException("No channel registered for type: " + type);
        }
        return channel;
    }
}

class UserPreferenceService {
    private final Map<String, Set<NotificationType>> userPreferences = new HashMap<>();

    void setPreferences(String userId, Set<NotificationType> types) {
        userPreferences.put(userId, types);
    }

    Set<NotificationType> getPreferences(String userId) {
        return userPreferences.getOrDefault(userId, Collections.emptySet());
    }
}

class NotificationRequestQueue {
    private final BlockingQueue<Notification> queue =
            new PriorityBlockingQueue<>(11, Comparator.comparingInt(n -> n.priority));

    void offer(Notification notification) {
        queue.offer(notification);
    }

    Notification take() throws InterruptedException {
        return queue.take();
    }
}

class NotificationDispatcher {
    private final NotificationRequestQueue queue = new NotificationRequestQueue();

    void enqueue(Notification notification) {
        queue.offer(notification);
    }

    Notification nextTask() throws InterruptedException {
        return queue.take();
    }
}

class NotificationWorker implements Runnable {
    private static final int MAX_RETRIES = 3;

    private final NotificationDispatcher dispatcher;
    private final UserPreferenceService preferenceService;
    private final NotificationChannelFactory channelFactory;
    private final ExecutorService deliveryPool;
    private volatile boolean running = true;

    NotificationWorker(NotificationDispatcher dispatcher, UserPreferenceService preferenceService,
                        NotificationChannelFactory channelFactory, ExecutorService deliveryPool) {
        this.dispatcher = dispatcher;
        this.preferenceService = preferenceService;
        this.channelFactory = channelFactory;
        this.deliveryPool = deliveryPool;
    }

    void stop() {
        running = false;
    }

    @Override
    public void run() {
        while (running) {
            Notification notification;
            try {
                notification = dispatcher.nextTask();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return;
            }

            Set<NotificationType> userPref = preferenceService.getPreferences(notification.recipientId);
            Set<NotificationType> channelsToUse = userPref.isEmpty() ? Set.of(notification.type) : userPref;

            for (NotificationType type : channelsToUse) {
                deliveryPool.submit(() -> deliverWithRetry(notification, type));
            }
        }
    }

    private void deliverWithRetry(Notification notification, NotificationType type) {
        NotificationChannel channel = channelFactory.getNotificationChannel(type);
        for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
            try {
                if (channel.sendNotification(notification)) {
                    notification.status = NotificationStatus.SENT;
                    return;
                }
            } catch (Exception e) {
                System.out.println("Attempt " + attempt + " failed: " + e.getMessage());
            }
        }
        notification.status = NotificationStatus.FAILED;
        System.out.println("Notification " + notification.id + " failed after " + MAX_RETRIES + " attempts");
    }
}

class NotificationService {
    private final NotificationDispatcher dispatcher;

    NotificationService(NotificationDispatcher dispatcher) {
        this.dispatcher = dispatcher;
    }

    boolean submitNotificationRequest(Notification notification) {
        dispatcher.enqueue(notification);
        return true;
    }
}

public class NotificationSystem_demo {
    public static void main(String[] args) throws InterruptedException {
        UserPreferenceService preferenceService = new UserPreferenceService();
        NotificationChannelFactory channelFactory = new NotificationChannelFactory();
        NotificationDispatcher dispatcher = new NotificationDispatcher();
        NotificationService notificationService = new NotificationService(dispatcher);

        User alice = new User("u1", "Alice");
        preferenceService.setPreferences("u1", Set.of(NotificationType.EMAIL, NotificationType.SMS));

        ExecutorService deliveryPool = Executors.newFixedThreadPool(4);
        NotificationWorker worker = new NotificationWorker(dispatcher, preferenceService, channelFactory, deliveryPool);
        Thread workerThread = new Thread(worker);
        workerThread.start();

        notificationService.submitNotificationRequest(
                new Notification("n1", alice, "u1", "Your order has shipped!", 1, NotificationType.EMAIL));
        notificationService.submitNotificationRequest(
                new Notification("n2", alice, "u1", "OTP: 4821", 0, NotificationType.SMS));

        Thread.sleep(500);
        worker.stop();
        workerThread.interrupt();
        deliveryPool.shutdown();
        deliveryPool.awaitTermination(2, TimeUnit.SECONDS);

        System.out.println("Done.");
    }
}
