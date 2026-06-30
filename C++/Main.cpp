/*
    ONE FILE - OOP Concepts in C++

    Covers:
    ✅ Class & Object
    ✅ Encapsulation
    ✅ Abstraction
    ✅ Inheritance
    ✅ Polymorphism
        - Method Overloading
        - Method Overriding
    ✅ Interface (pure virtual)
    ✅ Composition
    ✅ Association
    ✅ Aggregation
    ✅ Static
    ✅ Final (via convention / final keyword)
    ✅ this & super (:: scope)
*/

#include <iostream>
#include <string>
#include <memory>
using namespace std;

// ------------------------------------------------------------
// Abstraction
// ------------------------------------------------------------
class Employee {
protected:
    string name;
    double salary;
public:
    Employee(string name, double salary) : name(move(name)), salary(salary) {}
    virtual void work() = 0;
    void display() { cout << name << " earns " << salary << "\n"; }
    virtual ~Employee() = default;
};

// ------------------------------------------------------------
// Interface
// ------------------------------------------------------------
struct Payable {
    virtual void paySalary() = 0;
    virtual ~Payable() = default;
};

// ------------------------------------------------------------
// Inheritance + Method Overriding
// ------------------------------------------------------------
class Developer : public Employee, public Payable {
public:
    Developer(string name, double salary) : Employee(move(name), salary) {}

    void work() override {
        cout << name << " is writing Java code.\n";
    }

    void paySalary() override {
        cout << "Salary credited to " << name << "\n";
    }
};

// ------------------------------------------------------------
// Encapsulation
// ------------------------------------------------------------
class BankAccount {
    double balance;  // hidden data
public:
    explicit BankAccount(double balance) : balance(balance) {}

    void deposit(double amount)  { balance += amount; }
    void withdraw(double amount) { if (amount <= balance) balance -= amount; }
    double getBalance() const    { return balance; }
};

// ------------------------------------------------------------
// Composition
// ------------------------------------------------------------
class Engine {
public:
    void start() { cout << "Engine Started\n"; }
};

class Car {
    Engine engine;  // Strong Has-A relationship
public:
    void startCar() {
        engine.start();
        cout << "Car Started\n";
    }
};

// ------------------------------------------------------------
// Aggregation
// ------------------------------------------------------------
struct Department {
    string name;
    explicit Department(string name) : name(move(name)) {}
};

class Professor {
    string name;
    Department* department;  // Weak Has-A (not owned)
public:
    Professor(string name, Department* dept) : name(move(name)), department(dept) {}
    void showDepartment() {
        cout << name << " belongs to " << department->name << "\n";
    }
};

// ------------------------------------------------------------
// Association
// ------------------------------------------------------------
struct Course {
    string title;
    explicit Course(string title) : title(move(title)) {}
};

class Student {
    string name;
public:
    explicit Student(string name) : name(move(name)) {}
    void attend(const Course& course) {
        cout << name << " attends " << course.title << "\n";
    }
};

// ------------------------------------------------------------
// Polymorphism (Method Overloading)
// ------------------------------------------------------------
class Calculator {
public:
    int    add(int a, int b)           { return a + b; }
    double add(double a, double b)     { return a + b; }
    int    add(int a, int b, int c)    { return a + b + c; }
};

// ------------------------------------------------------------
// Static & Final
// ------------------------------------------------------------
class Company {
public:
    static string companyName;
    const int companyId;

    explicit Company(int id) : companyId(id) {}

    static void printCompany() { cout << companyName << "\n"; }
};
string Company::companyName = "SYS Titans";

// ------------------------------------------------------------
// this keyword demo
// ------------------------------------------------------------
class Person {
    string name;
public:
    explicit Person(string name) : name(move(name)) {}
    void print() { cout << this->name << "\n"; }
};

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main() {

    // Class & Object
    Person person("Pradeep");
    person.print();

    // Encapsulation
    BankAccount account(1000);
    account.deposit(500);
    account.withdraw(200);
    cout << "Balance = " << account.getBalance() << "\n";

    // Inheritance + Abstraction + Runtime Polymorphism
    unique_ptr<Employee> emp = make_unique<Developer>("Rahul", 120000);
    emp->display();
    emp->work();
    dynamic_cast<Payable*>(emp.get())->paySalary();

    // Method Overloading
    Calculator calculator;
    cout << calculator.add(2, 3) << "\n";
    cout << calculator.add(2.5, 3.5) << "\n";
    cout << calculator.add(1, 2, 3) << "\n";

    // Composition
    Car car;
    car.startCar();

    // Aggregation
    Department department("Computer Science");
    Professor professor("Amit", &department);
    professor.showDepartment();

    // Association
    Student student("Neha");
    Course course("Low Level Design");
    student.attend(course);

    // Static
    Company::printCompany();

    // Final
    Company company(101);
    cout << "Company ID = " << company.companyId << "\n";

    return 0;
}
