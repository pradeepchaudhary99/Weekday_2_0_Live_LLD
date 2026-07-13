package main

import "fmt"

type IDiscount interface {
	GetDiscountedPrice(amount int) float32
}

type Festival struct {
	discount float32
}

func NewFestival() *Festival {
	return &Festival{discount: 0.25}
}

func (f *Festival) GetDiscountedPrice(amount int) float32 {
	return float32(amount) * f.discount
}

type Holi struct {
	discount float32
}

func NewHoli() *Holi {
	return &Holi{discount: 0.25}
}

func (h *Holi) GetDiscountedPrice(amount int) float32 {
	return float32(amount) * h.discount
}

type PaymentService struct {
	discount IDiscount
}

func NewPaymentService(discount IDiscount) *PaymentService {
	return &PaymentService{discount: discount}
}

func (p *PaymentService) Pay(amount int) {
	final := p.discount.GetDiscountedPrice(amount)
	fmt.Println(final)
}

func main() {
	service := NewPaymentService(NewFestival()) // solution is Factory Method design Pattern
	_ = service
}
