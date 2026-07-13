package main

type Notification interface {
	Send(message string)
}

type NotificationSMS struct{}

func (n *NotificationSMS) Send(message string) {
	panic("Unimplemented method 'send'")
}

type NotificationSlack struct{}

func (n *NotificationSlack) Send(message string) {
	panic("Unimplemented method 'send'")
}

type NotificationServiceWithoutFactory struct{}

func (s *NotificationServiceWithoutFactory) SendNotification(notifType, message string) {
	if notifType == "SMS" {
		(&NotificationSMS{}).Send(message)
	} else if notifType == "Slack" {
		(&NotificationSlack{}).Send(message)
	}
}

// Simple Factory
func NotificationFactoryGetInstance(notifType, message string) Notification {
	if notifType == "SMS" {
		return &NotificationSMS{}
	} else if notifType == "Slack" {
		return &NotificationSlack{}
	}
	return &NotificationSMS{}
}

type NotificationServiceWithFactory struct{}

func (s *NotificationServiceWithFactory) SendNotification(notifType, message string) {
	notification := NotificationFactoryGetInstance(notifType, message)
	notification.Send(message)
}

func main() {}
