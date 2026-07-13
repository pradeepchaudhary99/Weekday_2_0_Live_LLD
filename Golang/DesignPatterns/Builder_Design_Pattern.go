package main

// Product

type Student struct {
	name       string // Mandatory
	rollNumber int    // Optional Parameter
	age        int    // Optional Parameter
	address    string // Optional Parameter
}

type StudentBuilder struct {
	name       string // Mandatory
	rollNumber int    // Optional Parameter
	age        int    // Optional Parameter
	address    string // Optional Parameter
}

func NewStudentBuilder(name string) *StudentBuilder {
	return &StudentBuilder{name: name}
}

func (b *StudentBuilder) SetRollNumber(rollNumber int) *StudentBuilder {
	b.rollNumber = rollNumber
	return b
}

func (b *StudentBuilder) SetAge(age int) *StudentBuilder {
	b.age = age
	return b
}

func (b *StudentBuilder) SetAddress(address string) *StudentBuilder {
	b.address = address
	return b
}

func (b *StudentBuilder) Build() *Student {
	return &Student{
		name:       b.name,
		rollNumber: b.rollNumber,
		age:        b.age,
		address:    b.address,
	}
}

func main() {
	student := NewStudentBuilder("pradeep").
		SetAddress("dada").
		SetAge(123).
		SetAddress("dads").
		Build()
	_ = student

	// Is modification of student possible..
}
