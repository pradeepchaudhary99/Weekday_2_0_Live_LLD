package main

import "fmt"

// =======================
// 1. SINGLE RESPONSIBILITY PRINCIPLE (SRP)
// =======================

// BAD: One struct doing multiple things
type UserBad struct {
	Name string
}

func (u *UserBad) SaveToDB() {
	fmt.Println("Saving user to DB")
}

func (u *UserBad) SendEmail() {
	fmt.Println("Sending email")
}

// GOOD: Separate responsibilities
type User struct {
	Name string
}

type UserRepository struct{}

func (r *UserRepository) Save(user *User) {
	fmt.Println("Saving user to DB")
}

type EmailService struct{}

func (e *EmailService) SendEmail(user *User) {
	fmt.Println("Sending email")
}

// =======================
// 2. OPEN CLOSED PRINCIPLE (OCP)
// =======================

// BAD: Need to modify function for new types
type DiscountCalculatorBad struct{}

func (d *DiscountCalculatorBad) Calculate(discountType string) float64 {
	switch discountType {
	case "NEW":
		return 10
	case "PREMIUM":
		return 20
	case "DIWALI":
		return 30
	}
	return 0
}

// GOOD: Extend without modifying
type DiscountStrategy interface {
	Calculate() float64
}

type NewCustomerDiscount struct{}

func (d *NewCustomerDiscount) Calculate() float64 { return 10 }

type PremiumCustomerDiscount struct{}

func (d *PremiumCustomerDiscount) Calculate() float64 { return 20 }

type DiscountCalculator struct{}

func (d *DiscountCalculator) Calculate(strategy DiscountStrategy) float64 {
	return strategy.Calculate()
}

// =======================
// 3. LISKOV SUBSTITUTION PRINCIPLE (LSP)
// =======================

// BAD: Violates substitution
type BirdBad struct{}

func (b *BirdBad) Fly() {
	fmt.Println("Flying")
}

type PenguinBad struct {
	BirdBad
}

func (p *PenguinBad) Fly() {
	panic("Can't fly")
}

// GOOD: Segregate flying behavior into a separate interface
type Bird interface{}

type FlyingBird interface {
	Bird
	Fly()
}

type Sparrow struct{}

func (s *Sparrow) Fly() {
	fmt.Println("Flying")
}

// =======================
// 4. INTERFACE SEGREGATION PRINCIPLE (ISP)
// =======================

// BAD: Fat interface
type WorkerBad interface {
	Work()
	Eat()
}

type RobotBad struct{}

func (r *RobotBad) Work() {
	fmt.Println("Working")
}

func (r *RobotBad) Eat() {
	panic("Robot doesn't eat")
}

// GOOD: Split interfaces
type Workable interface {
	Work()
}

type Eatable interface {
	Eat()
}

type Human struct{}

func (h *Human) Work() {
	fmt.Println("Working")
}

func (h *Human) Eat() {
	fmt.Println("Eating")
}

type Robot struct{}

func (r *Robot) Work() {
	fmt.Println("Working")
}

// =======================
// 5. DEPENDENCY INVERSION PRINCIPLE (DIP)
// =======================

// BAD: High-level depends on low-level
type MySQLDatabase struct{}

func (m *MySQLDatabase) Connect() {
	fmt.Println("Connecting to MySQL")
}

type ApplicationBad struct {
	db *MySQLDatabase
}

func NewApplicationBad() *ApplicationBad {
	return &ApplicationBad{db: &MySQLDatabase{}}
}

func (a *ApplicationBad) Start() {
	a.db.Connect()
}

// GOOD: Depend on abstraction
type Database interface {
	Connect()
}

type MySQL struct{}

func (m *MySQL) Connect() {
	fmt.Println("Connecting to MySQL")
}

type PostgreSQL struct{}

func (p *PostgreSQL) Connect() {
	fmt.Println("Connecting to PostgreSQL")
}

type Application struct {
	db Database
}

func NewApplication(db Database) *Application {
	return &Application{db: db}
}

func (a *Application) SetDB(db Database) {
	a.db = db
}

func (a *Application) Start() {
	a.db.Connect()
}

// =======================
// MAIN (TESTING)
// =======================
func main() {
	// SRP
	user := &User{}
	(&UserRepository{}).Save(user)
	(&EmailService{}).SendEmail(user)

	// OCP
	calc := &DiscountCalculator{}
	fmt.Println(calc.Calculate(&PremiumCustomerDiscount{}))

	// LSP
	var bird FlyingBird = &Sparrow{}
	bird.Fly()

	// ISP
	var robot Workable = &Robot{}
	robot.Work()

	// DIP
	app := NewApplication(&MySQL{})
	app.Start()
}
