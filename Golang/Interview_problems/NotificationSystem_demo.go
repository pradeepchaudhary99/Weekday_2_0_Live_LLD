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

package main

import (
	"container/heap"
	"fmt"
	"sync"
	"time"
)

type NotificationType int

const (
	SMS NotificationType = iota
	WHATSAPP
	EMAIL
)

type NotificationStatus int

const (
	PENDING NotificationStatus = iota
	SENT
	FAILED
)

type User struct {
	ID   string
	Name string
}

type Notification struct {
	ID          string
	User        *User
	RecipientID string
	Message     string
	Priority    int // lower value = higher priority
	Type        NotificationType
	Status      NotificationStatus
}

type NotificationChannel interface {
	SendNotification(notification *Notification) bool
}

type SMSNotificationChannel struct{}

func (c *SMSNotificationChannel) SendNotification(notification *Notification) bool {
	fmt.Printf("[SMS] to %s: %s\n", notification.RecipientID, notification.Message)
	return true
}

type WhatsappNotificationChannel struct{}

func (c *WhatsappNotificationChannel) SendNotification(notification *Notification) bool {
	fmt.Printf("[WhatsApp] to %s: %s\n", notification.RecipientID, notification.Message)
	return true
}

type EmailNotificationChannel struct{}

func (c *EmailNotificationChannel) SendNotification(notification *Notification) bool {
	fmt.Printf("[Email] to %s: %s\n", notification.RecipientID, notification.Message)
	return true
}

type NotificationChannelFactory struct {
	registry map[NotificationType]NotificationChannel
}

func NewNotificationChannelFactory() *NotificationChannelFactory {
	return &NotificationChannelFactory{
		registry: map[NotificationType]NotificationChannel{
			SMS:      &SMSNotificationChannel{},
			WHATSAPP: &WhatsappNotificationChannel{},
			EMAIL:    &EmailNotificationChannel{},
		},
	}
}

func (f *NotificationChannelFactory) GetNotificationChannel(t NotificationType) (NotificationChannel, error) {
	channel, ok := f.registry[t]
	if !ok {
		return nil, fmt.Errorf("no channel registered for type: %v", t)
	}
	return channel, nil
}

type UserPreferenceService struct {
	mu          sync.Mutex
	preferences map[string]map[NotificationType]bool
}

func NewUserPreferenceService() *UserPreferenceService {
	return &UserPreferenceService{preferences: make(map[string]map[NotificationType]bool)}
}

func (s *UserPreferenceService) SetPreferences(userID string, types []NotificationType) {
	s.mu.Lock()
	defer s.mu.Unlock()
	set := make(map[NotificationType]bool, len(types))
	for _, t := range types {
		set[t] = true
	}
	s.preferences[userID] = set
}

func (s *UserPreferenceService) GetPreferences(userID string) map[NotificationType]bool {
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.preferences[userID]
}

// entry wraps a notification (nil = poison pill) for the priority queue.
type entry struct {
	priority     int
	seq          int
	notification *Notification
}

type entryHeap []*entry

func (h entryHeap) Len() int { return len(h) }
func (h entryHeap) Less(i, j int) bool {
	if h[i].priority != h[j].priority {
		return h[i].priority < h[j].priority
	}
	return h[i].seq < h[j].seq
}
func (h entryHeap) Swap(i, j int)       { h[i], h[j] = h[j], h[i] }
func (h *entryHeap) Push(x interface{}) { *h = append(*h, x.(*entry)) }
func (h *entryHeap) Pop() interface{} {
	old := *h
	n := len(old)
	item := old[n-1]
	*h = old[:n-1]
	return item
}

// NotificationRequestQueue is a thread-safe priority queue keyed on Notification.Priority
// (lower = more urgent). A nil notification acts as the poison pill for shutdown.
type NotificationRequestQueue struct {
	mu   sync.Mutex
	cond *sync.Cond
	heap entryHeap
	seq  int
}

func NewNotificationRequestQueue() *NotificationRequestQueue {
	q := &NotificationRequestQueue{}
	q.cond = sync.NewCond(&q.mu)
	return q
}

func (q *NotificationRequestQueue) Offer(notification *Notification) {
	q.mu.Lock()
	priority := -1
	if notification != nil {
		priority = notification.Priority
	}
	heap.Push(&q.heap, &entry{priority: priority, seq: q.seq, notification: notification})
	q.seq++
	q.mu.Unlock()
	q.cond.Signal()
}

func (q *NotificationRequestQueue) Take() *Notification {
	q.mu.Lock()
	defer q.mu.Unlock()
	for q.heap.Len() == 0 {
		q.cond.Wait()
	}
	item := heap.Pop(&q.heap).(*entry)
	return item.notification
}

type NotificationDispatcher struct {
	queue *NotificationRequestQueue
}

func NewNotificationDispatcher() *NotificationDispatcher {
	return &NotificationDispatcher{queue: NewNotificationRequestQueue()}
}

func (d *NotificationDispatcher) Enqueue(notification *Notification) {
	d.queue.Offer(notification)
}

func (d *NotificationDispatcher) NextTask() *Notification {
	return d.queue.Take()
}

const maxRetries = 3

type NotificationWorker struct {
	dispatcher        *NotificationDispatcher
	preferenceService *UserPreferenceService
	channelFactory    *NotificationChannelFactory
	deliveryWG        sync.WaitGroup
}

func NewNotificationWorker(dispatcher *NotificationDispatcher, preferenceService *UserPreferenceService, channelFactory *NotificationChannelFactory) *NotificationWorker {
	return &NotificationWorker{
		dispatcher:        dispatcher,
		preferenceService: preferenceService,
		channelFactory:    channelFactory,
	}
}

func (w *NotificationWorker) Run() {
	for {
		notification := w.dispatcher.NextTask()
		if notification == nil { // poison pill
			w.deliveryWG.Wait()
			return
		}

		userPref := w.preferenceService.GetPreferences(notification.RecipientID)
		var channelsToUse []NotificationType
		if len(userPref) == 0 {
			channelsToUse = []NotificationType{notification.Type}
		} else {
			for t := range userPref {
				channelsToUse = append(channelsToUse, t)
			}
		}

		for _, t := range channelsToUse {
			w.deliveryWG.Add(1)
			go func(n *Notification, t NotificationType) {
				defer w.deliveryWG.Done()
				w.deliverWithRetry(n, t)
			}(notification, t)
		}
	}
}

func (w *NotificationWorker) deliverWithRetry(notification *Notification, t NotificationType) {
	channel, err := w.channelFactory.GetNotificationChannel(t)
	if err != nil {
		fmt.Println(err)
		notification.Status = FAILED
		return
	}

	for attempt := 1; attempt <= maxRetries; attempt++ {
		if channel.SendNotification(notification) {
			notification.Status = SENT
			return
		}
	}
	notification.Status = FAILED
	fmt.Printf("Notification %s failed after %d attempts\n", notification.ID, maxRetries)
}

type NotificationService struct {
	dispatcher *NotificationDispatcher
}

func NewNotificationService(dispatcher *NotificationDispatcher) *NotificationService {
	return &NotificationService{dispatcher: dispatcher}
}

func (s *NotificationService) SubmitNotificationRequest(notification *Notification) bool {
	s.dispatcher.Enqueue(notification)
	return true
}

func main() {
	preferenceService := NewUserPreferenceService()
	channelFactory := NewNotificationChannelFactory()
	dispatcher := NewNotificationDispatcher()
	notificationService := NewNotificationService(dispatcher)

	alice := &User{ID: "u1", Name: "Alice"}
	preferenceService.SetPreferences("u1", []NotificationType{EMAIL, SMS})

	worker := NewNotificationWorker(dispatcher, preferenceService, channelFactory)
	var workerWG sync.WaitGroup
	workerWG.Add(1)
	go func() {
		defer workerWG.Done()
		worker.Run()
	}()

	notificationService.SubmitNotificationRequest(&Notification{
		ID: "n1", User: alice, RecipientID: "u1", Message: "Your order has shipped!",
		Priority: 1, Type: EMAIL, Status: PENDING,
	})
	notificationService.SubmitNotificationRequest(&Notification{
		ID: "n2", User: alice, RecipientID: "u1", Message: "OTP: 4821",
		Priority: 0, Type: SMS, Status: PENDING,
	})

	time.Sleep(500 * time.Millisecond)
	dispatcher.Enqueue(nil) // poison pill to stop the worker
	workerWG.Wait()

	fmt.Println("Done.")
}
