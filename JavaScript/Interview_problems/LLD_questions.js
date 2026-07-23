'use strict';

// NotificationSystem
//
// 1. Understand the problem, ask clarifying questions
//
// 2. List down the functional and non functional requirements
//
// functional requirements:
// 1.
// 2.
// 3.
// 4.
// 5.
//
// Non-functional requirements:
// 1.
// 2.
// 3.
// 4.
//
// 3. Identifying the core entities and Relationship
//
// Identifying the Core Entity?
// Classes, Interfaces, Enums
//
// Notification
// NotificationChannel
// NotificationTypes
// ConcreateNotificationChannels{SMS, EMAIL}
//
// NotificationService
// NotificationDispatcher

const NotificationType = Object.freeze({
    SMS: 'SMS',
    EMAIL: 'EMAIL',
    SLACK: 'SLACK',
    WHATSAPP: 'WHATSAPP',
});

class Notification {
    constructor(type, id, message) {
        this.type = type;
        this.id = id;
        this.message = message;
    }
}

class NotificationChannel {
    sendNotification(notification) {
        throw new Error('sendNotification must be implemented by subclass');
    }
}

class SMSNotification extends NotificationChannel {
    sendNotification(notification) {
    }
}

class EmailNotification extends NotificationChannel {
    sendNotification(notification) {
    }
}

class SlackNotification extends NotificationChannel {
    sendNotification(notification) {
    }
}

class LLDQuestions {
}

// Posting Questions
//     --> go over internet... understand... functional non
//     Questions
//
//     2 Questions Everyday

// Next Class:

module.exports = {
    NotificationType,
    Notification,
    NotificationChannel,
    SMSNotification,
    EmailNotification,
    SlackNotification,
    LLDQuestions,
};
