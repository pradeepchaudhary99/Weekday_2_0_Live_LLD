package Java.DesignPatterns;

interface IPaymentProcessor{
    void pay(int amount);
}

class Razorpay implements IPaymentProcessor{

    @Override
    public void pay(int amount) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }
    
}

class ClearPay implements IPaymentProcessor{

    @Override
    public void pay(int amount) {
        // TODO Auto-generated method stub
        throw new UnsupportedOperationException("Unimplemented method 'pay'");
    }
    
}

interface IPaymentProcessorFactory{
    IPaymentProcessor getInstance();
}

class RazorPayFactory implements IPaymentProcessorFactory{
    @Override
    public IPaymentProcessor getInstance() {
        return new Razorpay();
    }
}

class ClearPayFactory implements IPaymentProcessorFactory{
    @Override
    public IPaymentProcessor getInstance() {
        return new ClearPay();
    }
}


class PaymentService{
    static IPaymentProcessor paymentProcessor;
    
    static void makeAmount(IPaymentProcessorFactory factory, int amount){
        paymentProcessor = factory.getInstance();
        paymentProcessor.pay(amount);
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
