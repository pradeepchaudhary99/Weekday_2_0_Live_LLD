package main

import "fmt"

// ------------------------------------------------------------
// ONE FILE - OOP Concepts in Go
//
// Covers:
// Struct & Object
// Encapsulation (unexported fields)
// Abstraction (interfaces)
// "Inheritance" (struct embedding)
// Polymorphism (interface dispatch; Go has no overloading)
// Interface
// Composition
// Association
// Aggregation
// Static (package-level vars/funcs)
// Final (const)
// ------------------------------------------------------------

// ------------------------------------------------------------
// Abstraction
// ------------------------------------------------------------
type Employee struct {
	name   string
	salary float64
}

func (e *Employee) Display() {
	fmt.Println(e.name, "earns", e.salary)
}

type Worker interface {
	Work()
}

// ------------------------------------------------------------
// Interface
// ------------------------------------------------------------
type Payable interface {
	PaySalary()
}

// ------------------------------------------------------------
// "Inheritance" via embedding + Method Overriding
// ------------------------------------------------------------
type Developer struct {
	Employee
}

func NewDeveloper(name string, salary float64) *Developer {
	return &Developer{Employee{name: name, salary: salary}}
}

func (d *Developer) Work() {
	fmt.Println(d.name, "is writing Go code.")
}

func (d *Developer) PaySalary() {
	fmt.Println("Salary credited to", d.name)
}

// ------------------------------------------------------------
// Encapsulation
// ------------------------------------------------------------
type BankAccount struct {
	balance float64 // hidden data
}

func NewBankAccount(balance float64) *BankAccount {
	return &BankAccount{balance: balance}
}

func (b *BankAccount) Deposit(amount float64) {
	b.balance += amount
}

func (b *BankAccount) Withdraw(amount float64) {
	if amount <= b.balance {
		b.balance -= amount
	}
}

func (b *BankAccount) GetBalance() float64 {
	return b.balance
}

// ------------------------------------------------------------
// Composition
// ------------------------------------------------------------
type Engine struct{}

func (e *Engine) Start() {
	fmt.Println("Engine Started")
}

type Car struct {
	engine Engine // Strong Has-A relationship
}

func (c *Car) StartCar() {
	c.engine.Start()
	fmt.Println("Car Started")
}

// ------------------------------------------------------------
// Aggregation
// ------------------------------------------------------------
type Department struct {
	Name string
}

type Professor struct {
	Name       string
	Department *Department // Weak Has-A relationship
}

func (p *Professor) ShowDepartment() {
	fmt.Println(p.Name, "belongs to", p.Department.Name)
}

// ------------------------------------------------------------
// Association
// ------------------------------------------------------------
type Student struct {
	Name string
}

type Course struct {
	Title string
}

func (s *Student) Attend(course *Course) {
	fmt.Println(s.Name, "attends", course.Title)
}

// ------------------------------------------------------------
// Polymorphism (Go has no method overloading; variadic args instead)
// ------------------------------------------------------------
type Calculator struct{}

func (c *Calculator) AddInts(nums ...int) int {
	total := 0
	for _, n := range nums {
		total += n
	}
	return total
}

func (c *Calculator) AddFloats(a, b float64) float64 {
	return a + b
}

// ------------------------------------------------------------
// Static & Final
// ------------------------------------------------------------
const CompanyName = "SYS Titans" // static + final

type Company struct {
	CompanyID int
}

func PrintCompany() {
	fmt.Println(CompanyName)
}

// ------------------------------------------------------------
// Person (no `this`/`self` in Go; receiver name plays that role)
// ------------------------------------------------------------
type Person struct {
	Name string
}

func (p *Person) Print() {
	fmt.Println(p.Name)
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
func main() {

	// Struct & Object
	person := &Person{Name: "Pradeep"}
	person.Print()

	// Encapsulation
	account := NewBankAccount(1000)
	account.Deposit(500)
	account.Withdraw(200)
	fmt.Println("Balance =", account.GetBalance())

	// "Inheritance" + Abstraction + Runtime Polymorphism
	dev := NewDeveloper("Rahul", 120000)
	dev.Display()

	var worker Worker = dev
	worker.Work()

	var payable Payable = dev
	payable.PaySalary()

	// Method Overloading (variadic / distinct methods in Go)
	calculator := &Calculator{}
	fmt.Println(calculator.AddInts(2, 3))
	fmt.Println(calculator.AddFloats(2.5, 3.5))
	fmt.Println(calculator.AddInts(1, 2, 3))

	// Composition
	car := &Car{}
	car.StartCar()

	// Aggregation
	department := &Department{Name: "Computer Science"}
	professor := &Professor{Name: "Amit", Department: department}
	professor.ShowDepartment()

	// Association
	student := &Student{Name: "Neha"}
	course := &Course{Title: "Low Level Design"}
	student.Attend(course)

	// Static
	PrintCompany()

	// Final
	company := &Company{CompanyID: 101}
	fmt.Println("Company ID =", company.CompanyID)
}
