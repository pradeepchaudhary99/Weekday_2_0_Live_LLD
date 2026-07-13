package Java;

// SOLID Principles Demonstration in ONE FILE

// =======================
// 1. SINGLE RESPONSIBILITY PRINCIPLE (SRP)
// =======================

// BAD: One class doing multiple things
class UserBad {
    String name;

    public void saveToDB() {
        System.out.println("Saving user to DB");
    }

    public void sendEmail() {
        System.out.println("Sending email");
    }
}

// GOOD: Separate responsibilities
class User {
    String name;
}

class UserRepository {
    public void save(User user) {
        System.out.println("Saving user to DB");
    }
}

class EmailService {
    public void sendEmail(User user) {
        System.out.println("Sending email");
    }
}


// =======================
// 2. OPEN CLOSED PRINCIPLE (OCP)
// =======================

// BAD: Need to modify class for new types
class DiscountCalculatorBad {
    public double calculate(String type) {
        if (type.equals("NEW")) return 10;
        else if (type.equals("PREMIUM")) return 20;
        else if(type.equals("DIWALI")) return 30;
        return 0;
    }
}

// GOOD: Extend without modifying
interface DiscountStrategy {
    double calculate();
}

class NewCustomerDiscount implements DiscountStrategy {
    public double calculate() { return 10; }
}

class PremiumCustomerDiscount implements DiscountStrategy {
    public double calculate() { return 20; }
}

//Dicoupling...
// 


class DiscountCalculator {
    public double calculate(DiscountStrategy strategy) {
        return strategy.calculate();
    }
}


// =======================
// 3. LISKOV SUBSTITUTION PRINCIPLE (LSP)
// =======================

// BAD: Violates substitution
class BirdBad {
    public void fly() {
        System.out.println("Flying");
    }
}

class PenguinBad extends BirdBad {
    public void fly() {
        throw new UnsupportedOperationException("Can't fly");
    }
}

// interface FlyingBird extends Bird {
//     void fly();
// }

// class Sparrow implements FlyingBird {
//     public void fly() {
//         System.out.println("Flying");
//     }
// }




// =======================
// 4. INTERFACE SEGREGATION PRINCIPLE (ISP)
// =======================

// BAD: Fat interface
interface WorkerBad {
    void work();
    void eat();
}

class RobotBad implements WorkerBad {
    public void work() {
        System.out.println("Working");
    }

    public void eat() {
        throw new UnsupportedOperationException("Robot doesn't eat");
    }
}

// GOOD: Split interfaces
interface Workable {
    void work();
}

interface Eatable {
    void eat();
}

class Human implements Workable, Eatable {
    public void work() {
        System.out.println("Working");
    }

    public void eat() {
        System.out.println("Eating");
    }
}

class Robot implements Workable {
    public void work() {
        System.out.println("Working");
    }
}


// =======================
// 5. DEPENDENCY INVERSION PRINCIPLE (DIP)
// =======================

// BAD: High-level depends on low-level
class MySQLDatabase {
    public void connect() {
        System.out.println("Connecting to MySQL");
    }
}

class ApplicationBad {
    MySQLDatabase db = new MySQLDatabase();

    public void start() {
        db.connect();
    }
}

// GOOD: Depend on abstraction
interface Database {
    void connect();
}

class MySQL implements Database {
    public void connect() {
        System.out.println("Connecting to MySQL");
    }
}

class PostgreSQL implements Database {
    public void connect() {
        System.out.println("Connecting to PostgreSQL");
    }
}

class Application {
    private Database db;

    public Application(Database db) {
        this.db = db;
    }

    public void setDB(Database db){
        this.db = db;
    }

    public void start() {
        db.connect();
    }
}


// =======================
// MAIN METHOD (TESTING)
// =======================
public class SolidDemo {
    public static void main(String[] args) {

        // SRP
        User user = new User();
        new UserRepository().save(user);
        new EmailService().sendEmail(user);

        // OCP
        DiscountCalculator calc = new DiscountCalculator();
        System.out.println(calc.calculate(new PremiumCustomerDiscount()));

        // LSP
        FlyingBird bird = new Sparrow();
        bird.fly();

        // ISP
        Workable robot = new Robot();
        robot.work();

        // DIP
        Application app = new Application(new MySQL());
        app.start();
    }
}