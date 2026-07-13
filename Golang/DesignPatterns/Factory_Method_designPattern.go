package main

type IPaymentProcessor interface {
	Pay(amount int)
}

type Razorpay struct{}

func (r *Razorpay) Pay(amount int) {
	panic("Unimplemented method 'pay'")
}

type ClearPay struct{}

func (c *ClearPay) Pay(amount int) {
	panic("Unimplemented method 'pay'")
}

type IPaymentProcessorFactory interface {
	GetInstance() IPaymentProcessor
}

type RazorPayFactory struct{}

func (f *RazorPayFactory) GetInstance() IPaymentProcessor {
	return &Razorpay{}
}

type ClearPayFactory struct{}

func (f *ClearPayFactory) GetInstance() IPaymentProcessor {
	return &ClearPay{}
}

var paymentProcessor IPaymentProcessor

func MakeAmount(factory IPaymentProcessorFactory, amount int) {
	paymentProcessor = factory.GetInstance()
	paymentProcessor.Pay(amount)
}

// Product Class
// Product Factory

func main() {
	MakeAmount(&ClearPayFactory{}, 122)
	MakeAmount(&RazorPayFactory{}, 122)
}
