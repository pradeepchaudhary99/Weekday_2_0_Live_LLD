
interface IDiscount{
    float getDiscountedPrice(int amount);
}

class Festival implements IDiscount{
    float discount = 0.25f;
    @Override
    public float getDiscountedPrice(int amount) {
        return amount*discount;
    }

}

class Holi implements IDiscount{
    float discount = 0.25f;
    @Override
    public float getDiscountedPrice(int amount) {
        return amount*discount;
    }
}

class PaymentService{
    IDiscount discount;

    public PaymentService(IDiscount discount){
        this.discount = discount;
    }
    void pay(int amount){
        float final_ = discount.getDiscountedPrice(amount);
        System.out.println(final_);
    }
}

public class OCP_Demo {
    public static void main(String[] args) {

        PaymentService service = new PaymentService(new Festival());    //solution is Factory Method design Pattern
    }
}
