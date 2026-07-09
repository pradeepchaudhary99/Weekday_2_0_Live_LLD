// Product

class StudentBuilder {
    constructor(name) {
        this.name = name; // Mandatory
        this.rollNumber = undefined; // Optional Parameter
        this.age = undefined; // Optional Parameter
        this.address = undefined; // Optional Parameter
    }

    setRollNumber(rollNumber) {
        this.rollNumber = rollNumber;
        return this;
    }
    setAge(age) {
        this.age = age;
        return this;
    }
    setAddress(address) {
        this.address = address;
        return this;
    }

    build() {
        return new Student(this);
    }
}

class Student {
    constructor(builder) {
        this.name = builder.name;
        this.rollNumber = builder.rollNumber;
        this.age = builder.age;
        this.address = builder.address;
    }

    static get StudentBuilder() {
        return StudentBuilder;
    }
}

class Builder_Design_Pattern {
    static main(args) {
        const student = new Student.StudentBuilder("pradeep")
            .setAddress("dada")
            .setAge(123)
            .setAddress("dads")
            .build();

        // Is modification of student possible..
    }
}

Builder_Design_Pattern.main([]);
