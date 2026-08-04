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

'use strict';

const NotificationType = Object.freeze({
  SMS: 'SMS',
  WHATSAPP: 'WHATSAPP',
  EMAIL: 'EMAIL',
});

const NotificationStatus = Object.freeze({
  PENDING: 'PENDING',
  SENT: 'SENT',
  FAILED: 'FAILED',
});

class User {
  constructor(id, name) {
    this.id = id;
    this.name = name;
  }
}

class Notification {
  constructor(id, user, recipientId, message, priority, type) {
    this.id = id;
    this.user = user;
    this.recipientId = recipientId;
    this.message = message;
    this.priority = priority; // lower value = higher priority
    this.type = type;
    this.status = NotificationStatus.PENDING;
  }
}

class NotificationChannel {
  sendNotification(_notification) {
    throw new Error('sendNotification must be implemented by subclasses');
  }
}

class SMSNotificationChannel extends NotificationChannel {
  sendNotification(notification) {
    console.log(`[SMS] to ${notification.recipientId}: ${notification.message}`);
    return true;
  }
}

class WhatsappNotificationChannel extends NotificationChannel {
  sendNotification(notification) {
    console.log(`[WhatsApp] to ${notification.recipientId}: ${notification.message}`);
    return true;
  }
}

class EmailNotificationChannel extends NotificationChannel {
  sendNotification(notification) {
    console.log(`[Email] to ${notification.recipientId}: ${notification.message}`);
    return true;
  }
}

class NotificationChannelFactory {
  constructor() {
    this.registry = new Map([
      [NotificationType.SMS, new SMSNotificationChannel()],
      [NotificationType.WHATSAPP, new WhatsappNotificationChannel()],
      [NotificationType.EMAIL, new EmailNotificationChannel()],
    ]);
  }

  getNotificationChannel(type) {
    const channel = this.registry.get(type);
    if (!channel) {
      throw new Error(`No channel registered for type: ${type}`);
    }
    return channel;
  }
}

class UserPreferenceService {
  constructor() {
    this.preferences = new Map();
  }

  setPreferences(userId, types) {
    this.preferences.set(userId, new Set(types));
  }

  getPreferences(userId) {
    return this.preferences.get(userId) || new Set();
  }
}

// Priority queue keyed on Notification.priority (lower = more urgent).
// A null notification acts as the poison pill used to stop the worker.
class NotificationRequestQueue {
  constructor() {
    this.items = []; // {priority, seq, notification}
    this.seq = 0;
  }

  offer(notification) {
    const priority = notification ? notification.priority : -1;
    this.items.push({ priority, seq: this.seq++, notification });
    this.items.sort((a, b) => a.priority - b.priority || a.seq - b.seq);
  }

  take() {
    const item = this.items.shift();
    return item ? item.notification : null;
  }

  isEmpty() {
    return this.items.length === 0;
  }
}

class NotificationDispatcher {
  constructor() {
    this.queue = new NotificationRequestQueue();
  }

  enqueue(notification) {
    this.queue.offer(notification);
  }

  nextTask() {
    return this.queue.take();
  }

  hasTask() {
    return !this.queue.isEmpty();
  }
}

const MAX_RETRIES = 3;

class NotificationWorker {
  constructor(dispatcher, preferenceService, channelFactory) {
    this.dispatcher = dispatcher;
    this.preferenceService = preferenceService;
    this.channelFactory = channelFactory;
    this.running = true;
    this.pendingDeliveries = [];
  }

  stop() {
    this.running = false;
    this.dispatcher.enqueue(null); // poison pill
  }

  // Node has no blocking queue, so the worker polls the dispatcher on a timer,
  // mirroring the blocking-take loop from the Java/C++/Go/Python versions.
  start() {
    return new Promise((resolve) => {
      const poll = () => {
        if (!this.dispatcher.hasTask()) {
          if (!this.running) {
            resolve();
            return;
          }
          setImmediate(poll);
          return;
        }

        const notification = this.dispatcher.nextTask();
        if (notification === null) {
          Promise.all(this.pendingDeliveries).then(() => resolve());
          return;
        }

        const userPref = this.preferenceService.getPreferences(notification.recipientId);
        const channelsToUse = userPref.size > 0 ? userPref : new Set([notification.type]);

        for (const type of channelsToUse) {
          this.pendingDeliveries.push(this.deliverWithRetry(notification, type));
        }

        setImmediate(poll);
      };
      poll();
    });
  }

  async deliverWithRetry(notification, type) {
    const channel = this.channelFactory.getNotificationChannel(type);
    for (let attempt = 1; attempt <= MAX_RETRIES; attempt++) {
      try {
        if (channel.sendNotification(notification)) {
          notification.status = NotificationStatus.SENT;
          return;
        }
      } catch (err) {
        console.log(`Attempt ${attempt} failed: ${err.message}`);
      }
    }
    notification.status = NotificationStatus.FAILED;
    console.log(`Notification ${notification.id} failed after ${MAX_RETRIES} attempts`);
  }
}

class NotificationService {
  constructor(dispatcher) {
    this.dispatcher = dispatcher;
  }

  submitNotificationRequest(notification) {
    this.dispatcher.enqueue(notification);
    return true;
  }
}

async function main() {
  const preferenceService = new UserPreferenceService();
  const channelFactory = new NotificationChannelFactory();
  const dispatcher = new NotificationDispatcher();
  const notificationService = new NotificationService(dispatcher);

  const alice = new User('u1', 'Alice');
  preferenceService.setPreferences('u1', [NotificationType.EMAIL, NotificationType.SMS]);

  const worker = new NotificationWorker(dispatcher, preferenceService, channelFactory);
  const workerDone = worker.start();

  notificationService.submitNotificationRequest(
    new Notification('n1', alice, 'u1', 'Your order has shipped!', 1, NotificationType.EMAIL)
  );
  notificationService.submitNotificationRequest(
    new Notification('n2', alice, 'u1', 'OTP: 4821', 0, NotificationType.SMS)
  );

  await new Promise((resolve) => setTimeout(resolve, 500));
  worker.stop();
  await workerDone;

  console.log('Done.');
}

main();
