#include <iostream>
#include <memory>

struct IDiscount {
    virtual float get_discounted_price(int amount) = 0;
    virtual ~IDiscount() = default;
};

struct Festival : IDiscount {
    float discount = 0.25f;
    float get_discounted_price(int amount) override {
        return amount * discount;
    }
};

struct Holi : IDiscount {
    float discount = 0.25f;
    float get_discounted_price(int amount) override {
        return amount * discount;
    }
};

class PaymentService {
    std::shared_ptr<IDiscount> discount;
public:
    explicit PaymentService(std::shared_ptr<IDiscount> d) : discount(std::move(d)) {}

    void pay(int amount) {
        float final_price = discount->get_discounted_price(amount);
        std::cout << final_price << std::endl;
    }
};

int main() {
    PaymentService service(std::make_shared<Festival>());  // solution is Factory Method design Pattern
    return 0;
}
