/*
    ONE FILE - OOP Concepts in JavaScript

    Covers:
    Class & Object
    Encapsulation
    Abstraction
    Inheritance
    Polymorphism
        - Method Overloading
        - Method Overriding
    Interface
    Composition
    Association
    Aggregation
    Static
    Final
    this & super
*/

//------------------------------------------------------------
// Abstraction
//------------------------------------------------------------
class Employee {
    constructor(name, salary) {
        if (new.target === Employee) {
            throw new Error("Cannot instantiate abstract class Employee");
        }
        this.name = name;
        this.salary = salary;
    }

    // Abstract Method
    work() {
        throw new Error("Not implemented");
    }

    // Concrete Method
    display() {
        console.log(this.name + " earns " + this.salary);
    }
}

//------------------------------------------------------------
// Interface
//------------------------------------------------------------
class Payable {
    paySalary() {
        throw new Error("Not implemented");
    }
}

//------------------------------------------------------------
// Inheritance + Method Overriding
//------------------------------------------------------------
class Developer extends Employee {
    constructor(name, salary) {
        super(name, salary); // super keyword
    }

    work() {
        console.log(this.name + " is writing Java code.");
    }

    paySalary() {
        console.log("Salary credited to " + this.name);
    }
}

//------------------------------------------------------------
// Encapsulation
//------------------------------------------------------------
class BankAccount {
    #balance; // hidden data

    constructor(balance) {
        this.#balance = balance;
    }

    deposit(amount) {
        this.#balance += amount;
    }

    withdraw(amount) {
        if (amount <= this.#balance)
            this.#balance -= amount;
    }

    getBalance() {
        return this.#balance;
    }
}

//------------------------------------------------------------
// Composition
//------------------------------------------------------------
class Engine {
    start() {
        console.log("Engine Started");
    }
}

class Car {
    // Strong Has-A relationship
    #engine = new Engine();

    startCar() {
        this.#engine.start();
        console.log("Car Started");
    }
}

//------------------------------------------------------------
// Aggregation
//------------------------------------------------------------
class Department {
    constructor(name) {
        this.name = name;
    }
}

class Professor {
    constructor(name, department) {
        this.name = name;
        // Weak Has-A relationship
        this.department = department;
    }

    showDepartment() {
        console.log(this.name + " belongs to " + this.department.name);
    }
}

//------------------------------------------------------------
// Association
//------------------------------------------------------------
class Student {
    constructor(name) {
        this.name = name;
    }

    attend(course) {
        console.log(this.name + " attends " + course.title);
    }
}

class Course {
    constructor(title) {
        this.title = title;
    }
}

//------------------------------------------------------------
// Polymorphism (Method Overloading)
//------------------------------------------------------------
class Calculator {
    // JS has no native method overloading; emulate via arguments/optional params.
    add(a, b, c) {
        if (c !== undefined) {
            return a + b + c;
        }
        return a + b;
    }
}

//------------------------------------------------------------
// Static & Final
//------------------------------------------------------------
class Company {
    constructor(id) {
        this.companyId = id; // final by convention
    }

    static printCompany() {
        console.log(Company.companyName);
    }
}
Company.companyName = "SYS Titans";

//------------------------------------------------------------
// this Keyword
//------------------------------------------------------------
class Person {
    constructor(name) {
        this.name = name; // this keyword
    }

    print() {
        console.log(this.name);
    }
}

//------------------------------------------------------------
// Main
//------------------------------------------------------------
class Main {
    static main(args) {

        //----------------------------------------------------
        // Class & Object
        //----------------------------------------------------
        const person = new Person("Pradeep");
        person.print();

        //----------------------------------------------------
        // Encapsulation
        //----------------------------------------------------
        const account = new BankAccount(1000);

        account.deposit(500);
        account.withdraw(200);

        console.log("Balance = " + account.getBalance());

        //----------------------------------------------------
        // Inheritance + Abstraction + Runtime Polymorphism
        //----------------------------------------------------
        const emp = new Developer("Rahul", 120000);

        emp.display();
        emp.work();

        const payable = emp;
        payable.paySalary();

        //----------------------------------------------------
        // Method Overloading
        //----------------------------------------------------
        const calculator = new Calculator();

        console.log(calculator.add(2, 3));
        console.log(calculator.add(2.5, 3.5));
        console.log(calculator.add(1, 2, 3));

        //----------------------------------------------------
        // Composition
        //----------------------------------------------------
        const car = new Car();
        car.startCar();

        //----------------------------------------------------
        // Aggregation
        //----------------------------------------------------
        const department = new Department("Computer Science");

        const professor = new Professor("Amit", department);
        professor.showDepartment();

        //----------------------------------------------------
        // Association
        //----------------------------------------------------
        const student = new Student("Neha");
        const course = new Course("Low Level Design");

        student.attend(course);

        //----------------------------------------------------
        // Static
        //----------------------------------------------------
        Company.printCompany();

        //----------------------------------------------------
        // Final
        //----------------------------------------------------
        const company = new Company(101);

        console.log("Company ID = " + company.companyId);
    }
}

Main.main([]);
