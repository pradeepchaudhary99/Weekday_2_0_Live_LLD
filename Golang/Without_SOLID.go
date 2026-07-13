package main

import (
	"fmt"
	"strings"
)

type Employee struct {
	id     int
	name   string
	typ    string
	salary float64
	email  string
}

func NewEmployee(id int, name, typ string, salary float64, email string) *Employee {
	return &Employee{id: id, name: name, typ: typ, salary: salary, email: email}
}

func (e *Employee) GetID() int         { return e.id }
func (e *Employee) GetName() string    { return e.name }
func (e *Employee) GetType() string    { return e.typ }
func (e *Employee) GetSalary() float64 { return e.salary }
func (e *Employee) GetEmail() string   { return e.email }

type EmployeeService struct{}

func (s *EmployeeService) ProcessEmployee(employee *Employee, notificationType string) {
	// =====================
	// Validation
	// =====================
	if employee.GetName() == "" {
		panic("Invalid Name")
	}
	if !strings.Contains(employee.GetEmail(), "@") {
		panic("Invalid Email")
	}

	// =====================
	// Salary Calculation
	// =====================
	var finalSalary float64

	switch employee.GetType() {
	case "FULL_TIME":
		finalSalary = employee.GetSalary()
	case "INTERN":
		finalSalary = employee.GetSalary() * 0.5
	case "CONTRACT":
		finalSalary = employee.GetSalary() * 1.2
	default:
		panic("Unknown Employee Type")
	}

	fmt.Println("Salary :", finalSalary)

	// =====================
	// Database
	// =====================
	fmt.Println("Connecting to MySQL...")
	fmt.Println("Saving Employee...")
	fmt.Println("Employee Saved")

	// =====================
	// Notification
	// =====================
	switch notificationType {
	case "EMAIL":
		fmt.Println("Sending Email to " + employee.GetEmail())
	case "SMS":
		fmt.Println("Sending SMS")
	case "WHATSAPP":
		fmt.Println("Sending WhatsApp Message")
	}

	// =====================
	// Audit
	// =====================
	fmt.Println("Writing Audit Log")

	// =====================
	// Analytics
	// =====================
	fmt.Println("Publishing Analytics Event")

	// =====================
	// Invoice
	// =====================
	fmt.Println("Generating Salary Slip")

	// =====================
	// Cloud Upload
	// =====================
	fmt.Println("Uploading Salary Slip to AWS S3")

	// =====================
	// Cache
	// =====================
	fmt.Println("Updating Redis Cache")

	// =====================
	// Report
	// =====================
	fmt.Println("Generating Monthly Report")

	fmt.Println("Employee Processing Completed")
}

func main() {
	service := &EmployeeService{}
	emp := NewEmployee(101, "Pradeep", "FULL_TIME", 100000, "pradeep@gmail.com")
	service.ProcessEmployee(emp, "EMAIL")
}
