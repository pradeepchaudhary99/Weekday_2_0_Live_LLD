/*
================================================================================
LLD: Notification System
================================================================================

Interview walkthrough (kept from the original notes, now answered):

1. Understand the problem, ask clarifying questions.
2. List the functional and non-functional requirements.
3. Identify the core entities and relationships.

--------------------------------------------------------------------------
Functional Requirements:
--------------------------------------------------------------------------
    1. Send a notification to a recipient through a specific channel
       (SMS, EMAIL, SLACK, WHATSAPP).
    2. New channels can be added without modifying existing channel code
       (Open/Closed Principle).
    3. A notification's delivery is tracked through explicit states:
       PENDING -> SENT, or PENDING -> FAILED after retries are exhausted.
    4. Transient channel failures are retried a bounded number of times
       before the notification is marked FAILED.

--------------------------------------------------------------------------
Non-Functional Requirements:
--------------------------------------------------------------------------
    1. Extensibility: adding WhatsApp/Slack/etc. is "write a new type", never
       "edit the dispatcher's switch statement".
    2. Reliability: a flaky channel gets a bounded number of retries, not
       infinite retries (which could wedge the dispatcher) and not zero
       retries (which would surface transient blips as hard failures).
    3. Decoupling: NotificationService (what to send / to whom) knows
       nothing about HOW a channel delivers a message; NotificationChannel
       implementations know nothing about retry policy.

--------------------------------------------------------------------------
Core Entities (Strategy + Registry pattern):
--------------------------------------------------------------------------
    NotificationType          -- SMS, EMAIL, SLACK, WHATSAPP
    NotificationStatus         -- PENDING, SENT, FAILED
    Notification               -- ID, Type, Recipient, Message, Status, Attempts
    NotificationChannel        -- strategy interface: Send(notification) bool
    Concrete channels          -- SMS/Email/WhatsApp (mock, always succeed),
                                   Slack (mock, fails a configurable number of
                                   times first, to exercise the retry path)
    NotificationDispatcher     -- registry of NotificationType -> NotificationChannel,
                                   owns the retry loop
    NotificationService        -- facade: creates a Notification and asks the
                                   dispatcher to deliver it
================================================================================
*/

package main

import (
	"fmt"
)

type NotificationType int

const (
	SMS NotificationType = iota
	EMAIL
	SLACK
	WHATSAPP
)

func (t NotificationType) String() string {
	switch t {
	case SMS:
		return "SMS"
	case EMAIL:
		return "EMAIL"
	case SLACK:
		return "SLACK"
	case WHATSAPP:
		return "WHATSAPP"
	}
	return "UNKNOWN"
}

type NotificationStatus int

const (
	PENDING NotificationStatus = iota
	SENT
	FAILED
)

func (s NotificationStatus) String() string {
	switch s {
	case PENDING:
		return "PENDING"
	case SENT:
		return "SENT"
	case FAILED:
		return "FAILED"
	}
	return "UNKNOWN"
}

var notificationIDCounter int

func nextNotificationID() string {
	notificationIDCounter++
	return fmt.Sprintf("notif-%d", notificationIDCounter)
}

type Notification struct {
	ID        string
	Type      NotificationType
	Recipient string
	Message   string
	Status    NotificationStatus
	Attempts  int
}

func NewNotification(notificationType NotificationType, recipient, message string) *Notification {
	return &Notification{
		ID:        nextNotificationID(),
		Type:      notificationType,
		Recipient: recipient,
		Message:   message,
		Status:    PENDING,
	}
}

type NotificationChannel interface {
	// Send returns true if the channel accepted/delivered the message.
	Send(notification *Notification) bool
}

type SMSNotificationChannel struct{}

func (c *SMSNotificationChannel) Send(notification *Notification) bool {
	fmt.Printf("[SMS] to %s: %s\n", notification.Recipient, notification.Message)
	return true
}

type EmailNotificationChannel struct{}

func (c *EmailNotificationChannel) Send(notification *Notification) bool {
	fmt.Printf("[EMAIL] to %s: %s\n", notification.Recipient, notification.Message)
	return true
}

type WhatsAppNotificationChannel struct{}

func (c *WhatsAppNotificationChannel) Send(notification *Notification) bool {
	fmt.Printf("[WHATSAPP] to %s: %s\n", notification.Recipient, notification.Message)
	return true
}

// SlackNotificationChannel simulates a channel with a flaky downstream
// provider: the first RemainingFailures calls fail, after which it starts
// succeeding. This exists purely to exercise NotificationDispatcher's
// retry loop deterministically, without relying on randomness.
type SlackNotificationChannel struct {
	RemainingFailures int
}

func (c *SlackNotificationChannel) Send(notification *Notification) bool {
	if c.RemainingFailures > 0 {
		c.RemainingFailures--
		fmt.Printf("[SLACK] delivery attempt failed (provider timeout) for %s\n", notification.Recipient)
		return false
	}
	fmt.Printf("[SLACK] to %s: %s\n", notification.Recipient, notification.Message)
	return true
}

type NotificationDispatcher struct {
	maxAttempts int
	channels    map[NotificationType]NotificationChannel
}

func NewNotificationDispatcher(maxAttempts int) *NotificationDispatcher {
	return &NotificationDispatcher{
		maxAttempts: maxAttempts,
		channels:    make(map[NotificationType]NotificationChannel),
	}
}

func (d *NotificationDispatcher) RegisterChannel(notificationType NotificationType, channel NotificationChannel) {
	d.channels[notificationType] = channel
}

func (d *NotificationDispatcher) Dispatch(notification *Notification) error {
	channel, ok := d.channels[notification.Type]
	if !ok {
		return fmt.Errorf("no channel registered for %s", notification.Type)
	}

	for notification.Attempts < d.maxAttempts {
		notification.Attempts++
		if channel.Send(notification) {
			notification.Status = SENT
			return nil
		}
	}
	notification.Status = FAILED
	return nil
}

type NotificationService struct {
	dispatcher    *NotificationDispatcher
	notifications map[string]*Notification
}

func NewNotificationService(dispatcher *NotificationDispatcher) *NotificationService {
	return &NotificationService{dispatcher: dispatcher, notifications: make(map[string]*Notification)}
}

func (s *NotificationService) CreateAndSend(notificationType NotificationType, recipient, message string) (*Notification, error) {
	notification := NewNotification(notificationType, recipient, message)
	s.notifications[notification.ID] = notification
	if err := s.dispatcher.Dispatch(notification); err != nil {
		return nil, err
	}
	return notification, nil
}

func main() {
	dispatcher := NewNotificationDispatcher(3)
	dispatcher.RegisterChannel(SMS, &SMSNotificationChannel{})
	dispatcher.RegisterChannel(EMAIL, &EmailNotificationChannel{})
	dispatcher.RegisterChannel(WHATSAPP, &WhatsAppNotificationChannel{})
	// Fails twice, then succeeds on the 3rd attempt -- within maxAttempts.
	dispatcher.RegisterChannel(SLACK, &SlackNotificationChannel{RemainingFailures: 2})

	service := NewNotificationService(dispatcher)

	sms, _ := service.CreateAndSend(SMS, "+1-555-0100", "Your OTP is 482913")
	email, _ := service.CreateAndSend(EMAIL, "alice@example.com", "Your order has shipped")
	whatsapp, _ := service.CreateAndSend(WHATSAPP, "+1-555-0200", "Your table is ready")

	fmt.Println("\n-- Sending a Slack notification through a flaky channel (retries expected) --")
	slack, _ := service.CreateAndSend(SLACK, "#alerts", "Build #482 failed")

	fmt.Println("\nFinal delivery report:")
	for _, n := range []*Notification{sms, email, whatsapp, slack} {
		fmt.Printf("  %-8s -> %-6s (attempts=%d)\n", n.Type, n.Status, n.Attempts)
	}
}
