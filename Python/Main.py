"""
ONE FILE - OOP Concepts in Python

Covers:
✅ Class & Object
✅ Encapsulation
✅ Abstraction
✅ Inheritance
✅ Polymorphism
    - Method Overloading (via default args / @overload)
    - Method Overriding
✅ Interface (via ABC)
✅ Composition
✅ Association
✅ Aggregation
✅ Static
✅ Final (via convention)
✅ self & super
"""

from abc import ABC, abstractmethod


# ------------------------------------------------------------
# Abstraction
# ------------------------------------------------------------
class Employee(ABC):
    def __init__(self, name: str, salary: float):
        self._name = name
        self._salary = salary

    @abstractmethod
    def work(self): pass

    def display(self):
        print(f"{self._name} earns {self._salary}")


# ------------------------------------------------------------
# Interface
# ------------------------------------------------------------
class Payable(ABC):
    @abstractmethod
    def pay_salary(self): pass


# ------------------------------------------------------------
# Inheritance + Method Overriding
# ------------------------------------------------------------
class Developer(Employee, Payable):
    def __init__(self, name: str, salary: float):
        super().__init__(name, salary)

    def work(self):
        print(f"{self._name} is writing Java code.")

    def pay_salary(self):
        print(f"Salary credited to {self._name}")


# ------------------------------------------------------------
# Encapsulation
# ------------------------------------------------------------
class BankAccount:
    def __init__(self, balance: float):
        self.__balance = balance  # hidden data

    def deposit(self, amount: float):
        self.__balance += amount

    def withdraw(self, amount: float):
        if amount <= self.__balance:
            self.__balance -= amount

    def get_balance(self) -> float:
        return self.__balance


# ------------------------------------------------------------
# Composition
# ------------------------------------------------------------
class Engine:
    def start(self):
        print("Engine Started")


class Car:
    def __init__(self):
        self.__engine = Engine()  # Strong Has-A relationship

    def start_car(self):
        self.__engine.start()
        print("Car Started")


# ------------------------------------------------------------
# Aggregation
# ------------------------------------------------------------
class Department:
    def __init__(self, name: str):
        self.name = name


class Professor:
    def __init__(self, name: str, department: Department):
        self.name = name
        self.department = department  # Weak Has-A relationship

    def show_department(self):
        print(f"{self.name} belongs to {self.department.name}")


# ------------------------------------------------------------
# Association
# ------------------------------------------------------------
class Course:
    def __init__(self, title: str):
        self.title = title


class Student:
    def __init__(self, name: str):
        self.name = name

    def attend(self, course: Course):
        print(f"{self.name} attends {course.title}")


# ------------------------------------------------------------
# Polymorphism (Method Overloading via default args)
# ------------------------------------------------------------
class Calculator:
    def add(self, a, b, c=None):
        if c is not None:
            return a + b + c
        return a + b


# ------------------------------------------------------------
# Static & Final
# ------------------------------------------------------------
class Company:
    company_name: str = "SYS Titans"  # static

    def __init__(self, company_id: int):
        self.company_id = company_id  # treated as final by convention

    @staticmethod
    def print_company():
        print(Company.company_name)


# ------------------------------------------------------------
# self keyword demo
# ------------------------------------------------------------
class Person:
    def __init__(self, name: str):
        self.name = name  # self keyword

    def print(self):
        print(self.name)


# ------------------------------------------------------------
# Main
# ------------------------------------------------------------
if __name__ == "__main__":

    # Class & Object
    person = Person("Pradeep")
    person.print()

    # Encapsulation
    account = BankAccount(1000)
    account.deposit(500)
    account.withdraw(200)
    print(f"Balance = {account.get_balance()}")

    # Inheritance + Abstraction + Runtime Polymorphism
    emp: Employee = Developer("Rahul", 120000)
    emp.display()
    emp.work()
    emp.pay_salary()

    # Method Overloading
    calculator = Calculator()
    print(calculator.add(2, 3))
    print(calculator.add(2.5, 3.5))
    print(calculator.add(1, 2, 3))

    # Composition
    car = Car()
    car.start_car()

    # Aggregation
    department = Department("Computer Science")
    professor = Professor("Amit", department)
    professor.show_department()

    # Association
    student = Student("Neha")
    course = Course("Low Level Design")
    student.attend(course)

    # Static
    Company.print_company()

    # Final
    company = Company(101)
    print(f"Company ID = {company.company_id}")
