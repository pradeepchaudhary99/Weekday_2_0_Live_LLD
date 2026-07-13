
interface IPaymentProcessor{
    void pay();
}

class Application implements IPaymentProcessor{
    @Override
    public void pay() {
        System.out.println("Legacy code");
    }
}

class StripeAdapter implements IPaymentProcessor{
    StripePayment stripePayment;
    @Override
    public void pay() {
        stripePayment.makePayment();
    }
}

class StripePayment{
    void makePayment(){
        System.out.println("Stripe is making payment");
    }
}

class PayTM{
    void makePaymentPaytm(){
        System.out.println("paytm karo");
    }
}

class PayTMAdapter implements IPaymentProcessor{
    PayTM paytm;

    @Override
    public void pay() {
        paytm.makePaymentPaytm();
    }

}

// class UniversalAdapter implements IPaymentProcessor{
//     PayTM payTM;
//     StripePayment stripe;
//     //sadsa
//     //dasda
//     //adsasddads
//     //dasdasdasd
//     //dasdasdada
//     //dsadasdasd

// }


public class AdapterDesign_pattern {
    public void clientCode(IPaymentProcessor process){
        process.pay();
    }
    public static void main(String[] args) {
        IPaymentProcessor process = new StripeAdapter();

        //--------------------
        process.pay();
    }
}
