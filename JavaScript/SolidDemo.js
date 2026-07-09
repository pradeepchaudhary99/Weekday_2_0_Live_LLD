// SOLID Principles Demonstration in ONE FILE

// =======================
// 1. SINGLE RESPONSIBILITY PRINCIPLE (SRP)
// =======================

// BAD: One class doing multiple things
class UserBad {
    constructor() {
        this.name = undefined;
    }

    saveToDB() {
        console.log("Saving user to DB");
    }

    sendEmail() {
        console.log("Sending email");
    }
}

// GOOD: Separate responsibilities
class User {
    constructor() {
        this.name = undefined;
    }
}

class UserRepository {
    save(user) {
        console.log("Saving user to DB");
    }
}

class EmailService {
    sendEmail(user) {
        console.log("Sending email");
    }
}


// =======================
// 2. OPEN CLOSED PRINCIPLE (OCP)
// =======================

// BAD: Need to modify class for new types
class DiscountCalculatorBad {
    calculate(type) {
        if (type === "NEW") return 10;
        else if (type === "PREMIUM") return 20;
        else if (type === "DIWALI") return 30;
        return 0;
    }
}

// GOOD: Extend without modifying
class DiscountStrategy {
    calculate() {
        throw new Error("Not implemented");
    }
}

class NewCustomerDiscount extends DiscountStrategy {
    calculate() { return 10; }
}

class PremiumCustomerDiscount extends DiscountStrategy {
    calculate() { return 20; }
}

// Decoupling...
//

class DiscountCalculator {
    calculate(strategy) {
        return strategy.calculate();
    }
}


// =======================
// 3. LISKOV SUBSTITUTION PRINCIPLE (LSP)
// =======================

// BAD: Violates substitution
class BirdBad {
    fly() {
        console.log("Flying");
    }
}

class PenguinBad extends BirdBad {
    fly() {
        throw new Error("Can't fly");
    }
}

// class FlyingBird extends Bird {
//     fly() {}
// }
//
// class Sparrow extends FlyingBird {
//     fly() {
//         console.log("Flying");
//     }
// }


// =======================
// 4. INTERFACE SEGREGATION PRINCIPLE (ISP)
// =======================

// BAD: Fat interface
class WorkerBad {
    work() {
        throw new Error("Not implemented");
    }
    eat() {
        throw new Error("Not implemented");
    }
}

class RobotBad extends WorkerBad {
    work() {
        console.log("Working");
    }

    eat() {
        throw new Error("Robot doesn't eat");
    }
}

// GOOD: Split interfaces
class Workable {
    work() {
        throw new Error("Not implemented");
    }
}

class Eatable {
    eat() {
        throw new Error("Not implemented");
    }
}

class Human extends Workable {
    work() {
        console.log("Working");
    }

    eat() {
        console.log("Eating");
    }
}

class Robot extends Workable {
    work() {
        console.log("Working");
    }
}


// =======================
// 5. DEPENDENCY INVERSION PRINCIPLE (DIP)
// =======================

// BAD: High-level depends on low-level
class MySQLDatabase {
    connect() {
        console.log("Connecting to MySQL");
    }
}

class ApplicationBad {
    constructor() {
        this.db = new MySQLDatabase();
    }

    start() {
        this.db.connect();
    }
}

// GOOD: Depend on abstraction
class Database {
    connect() {
        throw new Error("Not implemented");
    }
}

class MySQL extends Database {
    connect() {
        console.log("Connecting to MySQL");
    }
}

class PostgreSQL extends Database {
    connect() {
        console.log("Connecting to PostgreSQL");
    }
}

class Application {
    constructor(db) {
        this.db = db;
    }

    setDB(db) {
        this.db = db;
    }

    start() {
        this.db.connect();
    }
}


// =======================
// MAIN METHOD (TESTING)
// =======================
class SolidDemo {
    static main(args) {

        // SRP
        const user = new User();
        new UserRepository().save(user);
        new EmailService().sendEmail(user);

        // OCP
        const calc = new DiscountCalculator();
        console.log(calc.calculate(new PremiumCustomerDiscount()));

        // LSP
        // NOTE: pre-existing bug preserved from Java source — `FlyingBird`
        // and `Sparrow` are referenced here but never defined anywhere in
        // the original file (they only exist as commented-out code above).
        // This is a Java compile error; translated here it throws a
        // ReferenceError at runtime, which is the closest JS equivalent.
        const bird = new Sparrow();
        bird.fly();

        // ISP
        const robot = new Robot();
        robot.work();

        // DIP
        const app = new Application(new MySQL());
        app.start();
    }
}

SolidDemo.main([]);
