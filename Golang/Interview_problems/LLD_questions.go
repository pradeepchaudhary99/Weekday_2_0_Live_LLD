package main

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

type NotificationType int

const (
	SMS NotificationType = iota
	EMAIL
	SLACK
	WHATSAPP
)

type Notification struct {
	Type    NotificationType
	ID      string
	Message string
}

type NotificationChannel interface {
	SendNotification(notification Notification)
}

type SMSNotification struct{}

func (n *SMSNotification) SendNotification(notification Notification) {
}

type EmailNotification struct{}

func (n *EmailNotification) SendNotification(notification Notification) {
}

type SlackNotification struct{}

func (n *SlackNotification) SendNotification(notification Notification) {
}

// Posting Questions
//     --> go over internet... understand... functional non
//     Questions
//
//     2 Questions Everyday

// Next Class:

func main() {
}
