# Product

class Student:
    def __init__(self, builder: "Student.StudentBuilder"):
        self.name = builder.name
        self.roll_number = builder.roll_number
        self.age = builder.age
        self.address = builder.address

    class StudentBuilder:
        def __init__(self, name: str):
            self.name = name  # Mandatory
            self.roll_number: int = 0  # Optional Parameter
            self.age: int = 0  # Optional Parameter
            self.address: str = ""  # Optional Parameter

        def set_roll_number(self, roll_number: int) -> "Student.StudentBuilder":
            self.roll_number = roll_number
            return self

        def set_age(self, age: int) -> "Student.StudentBuilder":
            self.age = age
            return self

        def set_address(self, address: str) -> "Student.StudentBuilder":
            self.address = address
            return self

        def build(self) -> "Student":
            return Student(self)


if __name__ == "__main__":
    student = (
        Student.StudentBuilder("pradeep")
        .set_address("dada")
        .set_age(123)
        .set_address("dads")
        .build()
    )

    # Is modification of student possible..
