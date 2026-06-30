package Java;

/*
    ONE FILE - OOP Concepts in Java

    Covers:
    ✅ Class & Object
    ✅ Encapsulation
    ✅ Abstraction
    ✅ Inheritance
    ✅ Polymorphism
        - Method Overloading
        - Method Overriding
    ✅ Interface
    ✅ Composition
    ✅ Association
    ✅ Aggregation
    ✅ Static
    ✅ Final
    ✅ this & super
*/

import java.util.*;

//------------------------------------------------------------
// Abstraction
//------------------------------------------------------------
abstract class Employee {
    protected String name;
    protected double salary;

    public Employee(String name, double salary) {
        this.name = name;
        this.salary = salary;
    }

    // Abstract Method
    public abstract void work();

    // Concrete Method
    public void display() {
        System.out.println(name + " earns " + salary);
    }
}

//------------------------------------------------------------
// Interface
//------------------------------------------------------------
interface Payable {
    void paySalary();
}

//------------------------------------------------------------
// Inheritance + Method Overriding
//------------------------------------------------------------
class Developer extends Employee implements Payable {

    public Developer(String name, double salary) {
        super(name, salary);   // super keyword
    }

    @Override
    public void work() {
        System.out.println(name + " is writing Java code.");
    }

    @Override
    public void paySalary() {
        System.out.println("Salary credited to " + name);
    }
}

//------------------------------------------------------------
// Encapsulation
//------------------------------------------------------------
class BankAccount {

    private double balance;    // hidden data

    public BankAccount(double balance) {
        this.balance = balance;
    }

    public void deposit(double amount) {
        balance += amount;
    }

    public void withdraw(double amount) {
        if(amount <= balance)
            balance -= amount;
    }

    public double getBalance() {
        return balance;
    }
}

//------------------------------------------------------------
// Composition
//------------------------------------------------------------
class Engine {
    public void start() {
        System.out.println("Engine Started");
    }
}

class Car {

    // Strong Has-A relationship
    private Engine engine = new Engine();

    public void startCar() {
        engine.start();
        System.out.println("Car Started");
    }
}

//------------------------------------------------------------
// Aggregation
//------------------------------------------------------------
class Department {
    String name;

    Department(String name) {
        this.name = name;
    }
}

class Professor {

    String name;

    // Weak Has-A relationship
    Department department;

    Professor(String name, Department department) {
        this.name = name;
        this.department = department;
    }

    void showDepartment() {
        System.out.println(name + " belongs to " + department.name);
    }
}

//------------------------------------------------------------
// Association
//------------------------------------------------------------
class Student {

    String name;

    Student(String name) {
        this.name = name;
    }

    void attend(Course course) {
        System.out.println(name + " attends " + course.title);
    }
}

class Course {

    String title;

    Course(String title) {
        this.title = title;
    }
}

//------------------------------------------------------------
// Polymorphism (Method Overloading)
//------------------------------------------------------------
class Calculator {

    int add(int a, int b) {
        return a + b;
    }

    double add(double a, double b) {
        return a + b;
    }

    int add(int a, int b, int c) {
        return a + b + c;
    }
}

//------------------------------------------------------------
// Static & Final
//------------------------------------------------------------
class Company {

    static String companyName = "SYS Titans";

    final int companyId;

    Company(int id) {
        this.companyId = id;
    }

    static void printCompany() {
        System.out.println(companyName);
    }
}

//------------------------------------------------------------
// this Keyword
//------------------------------------------------------------
class Person {

    String name;

    Person(String name) {
        this.name = name;      // this keyword
    }

    void print() {
        System.out.println(this.name);
    }
}

//------------------------------------------------------------
// Main
//------------------------------------------------------------
public class Main {

    public static void main(String[] args) {

        //----------------------------------------------------
        // Class & Object
        //----------------------------------------------------
        Person person = new Person("Pradeep");
        person.print();

        //----------------------------------------------------
        // Encapsulation
        //----------------------------------------------------
        BankAccount account = new BankAccount(1000);

        account.deposit(500);
        account.withdraw(200);

        System.out.println("Balance = " + account.getBalance());

        //----------------------------------------------------
        // Inheritance + Abstraction + Runtime Polymorphism
        //----------------------------------------------------
        Employee emp = new Developer("Rahul", 120000);

        emp.display();
        emp.work();

        Payable payable = (Payable) emp;
        payable.paySalary();

        //----------------------------------------------------
        // Method Overloading
        //----------------------------------------------------
        Calculator calculator = new Calculator();

        System.out.println(calculator.add(2, 3));
        System.out.println(calculator.add(2.5, 3.5));
        System.out.println(calculator.add(1, 2, 3));

        //----------------------------------------------------
        // Composition
        //----------------------------------------------------
        Car car = new Car();
        car.startCar();

        //----------------------------------------------------
        // Aggregation
        //----------------------------------------------------
        Department department = new Department("Computer Science");

        Professor professor = new Professor("Amit", department);
        professor.showDepartment();

        //----------------------------------------------------
        // Association
        //----------------------------------------------------
        Student student = new Student("Neha");
        Course course = new Course("Low Level Design");

        student.attend(course);

        //----------------------------------------------------
        // Static
        //----------------------------------------------------
        Company.printCompany();

        //----------------------------------------------------
        // Final
        //----------------------------------------------------
        Company company = new Company(101);

        System.out.println("Company ID = " + company.companyId);
    }
}