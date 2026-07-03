#include <iostream>
#include <string>
#include <stdexcept>

class Employee {
    int id;
    std::string name;
    std::string type;
    double salary;
    std::string email;

public:
    Employee(int id, const std::string& name, const std::string& type, double salary, const std::string& email)
        : id(id), name(name), type(type), salary(salary), email(email) {}

    int get_id() const { return id; }
    const std::string& get_name() const { return name; }
    const std::string& get_type() const { return type; }
    double get_salary() const { return salary; }
    const std::string& get_email() const { return email; }
};

class EmployeeService {
public:
    void process_employee(const Employee& employee, const std::string& notification_type) {
        // =====================
        // Validation
        // =====================
        if (employee.get_name().empty()) {
            throw std::runtime_error("Invalid Name");
        }
        if (employee.get_email().find('@') == std::string::npos) {
            throw std::runtime_error("Invalid Email");
        }

        // =====================
        // Salary Calculation
        // =====================
        double final_salary = 0;
        const std::string& emp_type = employee.get_type();
        if (emp_type == "FULL_TIME") {
            final_salary = employee.get_salary();
        } else if (emp_type == "INTERN") {
            final_salary = employee.get_salary() * 0.5;
        } else if (emp_type == "CONTRACT") {
            final_salary = employee.get_salary() * 1.2;
        } else {
            throw std::runtime_error("Unknown Employee Type");
        }

        std::cout << "Salary : " << final_salary << std::endl;

        // =====================
        // Database
        // =====================
        std::cout << "Connecting to MySQL..." << std::endl;
        std::cout << "Saving Employee..." << std::endl;
        std::cout << "Employee Saved" << std::endl;

        // =====================
        // Notification
        // =====================
        if (notification_type == "EMAIL") {
            std::cout << "Sending Email to " << employee.get_email() << std::endl;
        } else if (notification_type == "SMS") {
            std::cout << "Sending SMS" << std::endl;
        } else if (notification_type == "WHATSAPP") {
            std::cout << "Sending WhatsApp Message" << std::endl;
        }

        // =====================
        // Audit
        // =====================
        std::cout << "Writing Audit Log" << std::endl;

        // =====================
        // Analytics
        // =====================
        std::cout << "Publishing Analytics Event" << std::endl;

        // =====================
        // Invoice
        // =====================
        std::cout << "Generating Salary Slip" << std::endl;

        // =====================
        // Cloud Upload
        // =====================
        std::cout << "Uploading Salary Slip to AWS S3" << std::endl;

        // =====================
        // Cache
        // =====================
        std::cout << "Updating Redis Cache" << std::endl;

        // =====================
        // Report
        // =====================
        std::cout << "Generating Monthly Report" << std::endl;

        std::cout << "Employee Processing Completed" << std::endl;
    }
};

int main() {
    EmployeeService service;
    Employee emp(101, "Pradeep", "FULL_TIME", 100000, "pradeep@gmail.com");
    service.process_employee(emp, "EMAIL");
    return 0;
}
