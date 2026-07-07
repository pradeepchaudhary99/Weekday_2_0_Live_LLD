package Java.DesignPatterns;

//Product Interfaces 
interface IPaymentProcessor{
    void pay(int amount);
}

interface IRefundProcessor{
    void refund();
}

interface InvoiceProcessor{
    void pay();
}


class RazorpayPayment implements IPaymentProcessor{
    @Override
    public void pay(int amount) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }
}

class RazorpayRefund implements IRefundProcessor{
    @Override
    public void refund() {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }
}

class RazorpayInvoice implements InvoiceProcessor{
    @Override
    public void pay() {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }
}

class ClearPayPayment implements IPaymentProcessor{

    @Override
    public void pay(int amount) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }
}

class ClearPayRefund implements IRefundProcessor{
    @Override
    public void refund() {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }   
}

class ClearPayInvoice implements InvoiceProcessor{
    @Override
    public void pay() {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }
}

interface IPaymentGateway_Factory{
    IPaymentProcessor getPayment();
    IRefundProcessor getRefund();
    InvoiceProcessor getInvoice();
}




class RazorPayFactory implements IPaymentGateway_Factory{

    @Override
    public IPaymentProcessor getPayment() {
        return new RazorpayPayment();
    }

    @Override
    public IRefundProcessor getRefund() {
        return new RazorpayRefund();
    }

    @Override
    public InvoiceProcessor getInvoice() {
        return new RazorpayInvoice();
    }
}

class ClearPayFactory implements IPaymentGateway_Factory{

    @Override
    public IPaymentProcessor getPayment() {
        return new ClearPayPayment();
    }

    @Override
    public IRefundProcessor getRefund() {
        return new ClearPayRefund();
    }

    @Override
    public InvoiceProcessor getInvoice() {
        return new ClearPayInvoice();
    }
}


class PaymentService{
    static void ProcessOrderPayment(IPaymentGateway_Factory factory, int amount){
        IPaymentProcessor payment = factory.getPayment();
        IRefundProcessor refund = factory.getRefund();
        InvoiceProcessor invoice = factory.getInvoice();
    }
}
// Product Class
// Product Factory


public class Factory_Method_designPattern {
    public static void main(String[] args) {
         PaymentService.makeAmount(new ClearPayFactory(), 122);
         PaymentService.makeAmount(new RazorPayFactory(), 122);

    }
}
