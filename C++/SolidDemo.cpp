#include <iostream>
#include <memory>
#include <string>
#include <stdexcept>


// =======================
// 1. SINGLE RESPONSIBILITY PRINCIPLE (SRP)
// =======================

// BAD: One class doing multiple things
class UserBad {
public:
    std::string name;

    void save_to_db() {
        std::cout << "Saving user to DB\n";
    }

    void send_email() {
        std::cout << "Sending email\n";
    }
};

// GOOD: Separate responsibilities
struct User {
    std::string name;
};

class UserRepository {
public:
    void save(const User& user) {
        std::cout << "Saving user to DB\n";
    }
};

class EmailService {
public:
    void send_email(const User& user) {
        std::cout << "Sending email\n";
    }
};


// =======================
// 2. OPEN CLOSED PRINCIPLE (OCP)
// =======================

// BAD: Need to modify class for new types
class DiscountCalculatorBad {
public:
    double calculate(const std::string& type) {
        if (type == "NEW") return 10;
        else if (type == "PREMIUM") return 20;
        else if (type == "DIWALI") return 30;
        return 0;
    }
};

// GOOD: Extend without modifying
struct DiscountStrategy {
    virtual double calculate() const = 0;
    virtual ~DiscountStrategy() = default;
};

class NewCustomerDiscount : public DiscountStrategy {
public:
    double calculate() const override { return 10; }
};

class PremiumCustomerDiscount : public DiscountStrategy {
public:
    double calculate() const override { return 20; }
};

class DiscountCalculator {
public:
    double calculate(const DiscountStrategy& strategy) {
        return strategy.calculate();
    }
};


// =======================
// 3. LISKOV SUBSTITUTION PRINCIPLE (LSP)
// =======================

// BAD: Violates substitution
class BirdBad {
public:
    virtual void fly() {
        std::cout << "Flying\n";
    }
    virtual ~BirdBad() = default;
};

class PenguinBad : public BirdBad {
public:
    void fly() override {
        throw std::runtime_error("Can't fly");
    }
};

// GOOD: Segregate flying behavior
struct Bird {
    virtual ~Bird() = default;
};

struct FlyingBird : public Bird {
    virtual void fly() = 0;
};

class Sparrow : public FlyingBird {
public:
    void fly() override {
        std::cout << "Flying\n";
    }
};


// =======================
// 4. INTERFACE SEGREGATION PRINCIPLE (ISP)
// =======================

// BAD: Fat interface
struct WorkerBad {
    virtual void work() = 0;
    virtual void eat() = 0;
    virtual ~WorkerBad() = default;
};

class RobotBad : public WorkerBad {
public:
    void work() override {
        std::cout << "Working\n";
    }

    void eat() override {
        throw std::runtime_error("Robot doesn't eat");
    }
};

// GOOD: Split interfaces
struct Workable {
    virtual void work() = 0;
    virtual ~Workable() = default;
};

struct Eatable {
    virtual void eat() = 0;
    virtual ~Eatable() = default;
};

class Human : public Workable, public Eatable {
public:
    void work() override {
        std::cout << "Working\n";
    }

    void eat() override {
        std::cout << "Eating\n";
    }
};

class Robot : public Workable {
public:
    void work() override {
        std::cout << "Working\n";
    }
};


// =======================
// 5. DEPENDENCY INVERSION PRINCIPLE (DIP)
// =======================

// BAD: High-level depends on low-level
class MySQLDatabase {
public:
    void connect() {
        std::cout << "Connecting to MySQL\n";
    }
};

class ApplicationBad {
    MySQLDatabase db;
public:
    void start() {
        db.connect();
    }
};

// GOOD: Depend on abstraction
struct Database {
    virtual void connect() = 0;
    virtual ~Database() = default;
};

class MySQL : public Database {
public:
    void connect() override {
        std::cout << "Connecting to MySQL\n";
    }
};

class PostgreSQL : public Database {
public:
    void connect() override {
        std::cout << "Connecting to PostgreSQL\n";
    }
};

class Application {
    std::unique_ptr<Database> db;
public:
    explicit Application(std::unique_ptr<Database> db) : db(std::move(db)) {}

    void start() {
        db->connect();
    }
};


// =======================
// MAIN (TESTING)
// =======================
int main() {
    // SRP
    User user;
    UserRepository().save(user);
    EmailService().send_email(user);

    // OCP
    DiscountCalculator calc;
    std::cout << calc.calculate(PremiumCustomerDiscount()) << "\n";

    // LSP
    std::unique_ptr<FlyingBird> bird = std::make_unique<Sparrow>();
    bird->fly();

    // ISP
    std::unique_ptr<Workable> robot = std::make_unique<Robot>();
    robot->work();

    // DIP
    Application app(std::make_unique<MySQL>());
    app.start();

    return 0;
}
