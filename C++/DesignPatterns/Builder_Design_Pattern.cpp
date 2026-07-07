#include <string>

// Product

class Student {
public:
    class StudentBuilder {
    public:
        std::string name;      // Mandatory
        int rollNumber = 0;    // Optional Parameter
        int age = 0;           // Optional Parameter
        std::string address;   // Optional Parameter

        explicit StudentBuilder(const std::string& name) : name(name) {}

        StudentBuilder& setRollNumber(int rollNumber) {
            this->rollNumber = rollNumber;
            return *this;
        }

        StudentBuilder& setAge(int age) {
            this->age = age;
            return *this;
        }

        StudentBuilder& setAddress(const std::string& address) {
            this->address = address;
            return *this;
        }

        Student build() {
            return Student(*this);
        }
    };

    explicit Student(const StudentBuilder& builder)
        : name(builder.name),
          rollNumber(builder.rollNumber),
          age(builder.age),
          address(builder.address) {}

private:
    std::string name;
    int rollNumber;
    int age;
    std::string address;
};

int main() {
    Student student = Student::StudentBuilder("pradeep")
                           .setAddress("dada")
                           .setAge(123)
                           .setAddress("dads")
                           .build();

    // Is modification of student possible..

    return 0;
}
