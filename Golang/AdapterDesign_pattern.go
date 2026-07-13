package main

import "fmt"

type IPaymentProcessor interface {
	Pay()
}

type Application struct{}

func (a *Application) Pay() {
	fmt.Println("Legacy code")
}

type StripePayment struct{}

func (s *StripePayment) MakePayment() {
	fmt.Println("Stripe is making payment")
}

type StripeAdapter struct {
	stripePayment *StripePayment
}

func NewStripeAdapter(stripePayment *StripePayment) *StripeAdapter {
	return &StripeAdapter{stripePayment: stripePayment}
}

func (s *StripeAdapter) Pay() {
	s.stripePayment.MakePayment()
}

type PayTM struct{}

func (p *PayTM) MakePaymentPaytm() {
	fmt.Println("paytm karo")
}

type PayTMAdapter struct {
	paytm *PayTM
}

func NewPayTMAdapter(paytm *PayTM) *PayTMAdapter {
	return &PayTMAdapter{paytm: paytm}
}

func (p *PayTMAdapter) Pay() {
	p.paytm.MakePaymentPaytm()
}

func clientCode(process IPaymentProcessor) {
	process.Pay()
}

func main() {
	var process IPaymentProcessor = NewStripeAdapter(&StripePayment{})

	process.Pay()
}
