package main

import "fmt"

type Notification interface {
	Send(message string)
}

type SMSNotification struct{}

func (s *SMSNotification) Send(message string) {
	fmt.Println("SMS is Sent: message" + message)
}

type Whatsapp struct{}

func (w *Whatsapp) Send(message string) {
	fmt.Println("whatsapp is Sent: message" + message)
}

type Decorator struct {
	wrappedObject Notification
}

type RateLimiter struct {
	Decorator
}

func NewRateLimiter(wrapped Notification) *RateLimiter {
	return &RateLimiter{Decorator{wrappedObject: wrapped}}
}

func (r *RateLimiter) Send(message string) {
	fmt.Println("Logic of Rate limiting 1000 Lines")
	fmt.Println("Logic of Rate limiting 1000 Lines")
	fmt.Println("Logic of Rate limiting 1000 Lines")
	fmt.Println("Logic of Rate limiting 1000 Lines")
	fmt.Println("Logic of Rate limiting 1000 Lines")
	r.wrappedObject.Send(message)
}

type LoadBalancer struct {
	Decorator
}

func NewLoadBalancer(wrapped Notification) *LoadBalancer {
	return &LoadBalancer{Decorator{wrappedObject: wrapped}}
}

func (l *LoadBalancer) Send(message string) {
	fmt.Println("Logic Load balancer 1000 Lines")
	l.wrappedObject.Send(message)
}

type JSonFormatter struct {
	Decorator
}

func NewJSonFormatter(wrapped Notification) *JSonFormatter {
	return &JSonFormatter{Decorator{wrappedObject: wrapped}}
}

func (j *JSonFormatter) Send(message string) {
	fmt.Println("Logic Json 1000 Lines")
	j.wrappedObject.Send(message)
}

type WhatsappFormtter struct {
	Decorator
}

func NewWhatsappFormtter(wrapped Notification) *WhatsappFormtter {
	return &WhatsappFormtter{Decorator{wrappedObject: wrapped}}
}

func (w *WhatsappFormtter) Send(message string) {
	fmt.Println("whatsapp formetter")
	w.wrappedObject.Send(message)
}

func main() {
	var notification2 Notification = NewWhatsappFormtter(NewWhatsappFormtter(NewRateLimiter(&Whatsapp{})))
	notification2.Send("pradeep")
}
