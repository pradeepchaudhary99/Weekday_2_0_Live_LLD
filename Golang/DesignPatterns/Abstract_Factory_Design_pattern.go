package main

// Product Interfaces

type IPaymentProcessor interface {
	Pay(amount int)
}

type IRefundProcessor interface {
	Refund()
}

type InvoiceProcessor interface {
	Pay()
}

type RazorpayPayment struct{}

func (r *RazorpayPayment) Pay(amount int) {
	panic("Unimplemented method 'pay'")
}

type RazorpayRefund struct{}

func (r *RazorpayRefund) Refund() {
	panic("Unimplemented method 'pay'")
}

type RazorpayInvoice struct{}

func (r *RazorpayInvoice) Pay() {
	panic("Unimplemented method 'pay'")
}

type ClearPayPayment struct{}

func (c *ClearPayPayment) Pay(amount int) {
	panic("Unimplemented method 'pay'")
}

type ClearPayRefund struct{}

func (c *ClearPayRefund) Refund() {
	panic("Unimplemented method 'pay'")
}

type ClearPayInvoice struct{}

func (c *ClearPayInvoice) Pay() {
	panic("Unimplemented method 'pay'")
}

type IPaymentGatewayFactory interface {
	GetPayment() IPaymentProcessor
	GetRefund() IRefundProcessor
	GetInvoice() InvoiceProcessor
}

type RazorPayFactory struct{}

func (f *RazorPayFactory) GetPayment() IPaymentProcessor {
	return &RazorpayPayment{}
}

func (f *RazorPayFactory) GetRefund() IRefundProcessor {
	return &RazorpayRefund{}
}

func (f *RazorPayFactory) GetInvoice() InvoiceProcessor {
	return &RazorpayInvoice{}
}

type ClearPayFactory struct{}

func (f *ClearPayFactory) GetPayment() IPaymentProcessor {
	return &ClearPayPayment{}
}

func (f *ClearPayFactory) GetRefund() IRefundProcessor {
	return &ClearPayRefund{}
}

func (f *ClearPayFactory) GetInvoice() InvoiceProcessor {
	return &ClearPayInvoice{}
}

func ProcessOrderPayment(factory IPaymentGatewayFactory, amount int) {
	payment := factory.GetPayment()
	refund := factory.GetRefund()
	invoice := factory.GetInvoice()
	_ = payment
	_ = refund
	_ = invoice
}

// Product Class
// Product Factory

func main() {
	ProcessOrderPayment(&ClearPayFactory{}, 122)
	ProcessOrderPayment(&RazorPayFactory{}, 122)
}
