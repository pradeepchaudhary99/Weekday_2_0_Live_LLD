class Employee {
    constructor(id, name, type, salary, email) {
        this.id = id;
        this.name = name;
        this.type = type;
        this.salary = salary;
        this.email = email;
    }

    getId() {
        return this.id;
    }

    getName() {
        return this.name;
    }

    getType() {
        return this.type;
    }

    getSalary() {
        return this.salary;
    }

    getEmail() {
        return this.email;
    }
}

class EmployeeService {

    processEmployee(employee, notificationType) {

        // =====================
        // Validation
        // =====================

        if (employee.getName() == null || employee.getName() === "") {
            throw new Error("Invalid Name");
        }

        if (!employee.getEmail().includes("@")) {
            throw new Error("Invalid Email");
        }

        // =====================
        // Salary Calculation
        // =====================

        let finalSalary = 0;

        if (employee.getType() === "FULL_TIME") {
            finalSalary = employee.getSalary();
        }
        else if (employee.getType() === "INTERN") {
            finalSalary = employee.getSalary() * 0.5;
        }
        else if (employee.getType() === "CONTRACT") {
            finalSalary = employee.getSalary() * 1.2;
        }
        else {
            throw new Error("Unknown Employee Type");
        }

        console.log("Salary : " + finalSalary);

        // =====================
        // Database
        // =====================

        console.log("Connecting to MySQL...");
        console.log("Saving Employee...");
        console.log("Employee Saved");

        // =====================
        // Notification
        // =====================

        if (notificationType === "EMAIL") {
            console.log("Sending Email to " + employee.getEmail());
        }
        else if (notificationType === "SMS") {
            console.log("Sending SMS");
        }
        else if (notificationType === "WHATSAPP") {
            console.log("Sending WhatsApp Message");
        }

        // =====================
        // Audit
        // =====================

        console.log("Writing Audit Log");

        // =====================
        // Analytics
        // =====================

        console.log("Publishing Analytics Event");

        // =====================
        // Invoice
        // =====================

        console.log("Generating Salary Slip");

        // =====================
        // Cloud Upload
        // =====================

        console.log("Uploading Salary Slip to AWS S3");

        // =====================
        // Cache
        // =====================

        console.log("Updating Redis Cache");

        // =====================
        // Report
        // =====================

        console.log("Generating Monthly Report");

        console.log("Employee Processing Completed");
    }
}

class Without_SOLID {
    static main(args) {

        const service = new EmployeeService();

        const emp = new Employee(
            101,
            "Pradeep",
            "FULL_TIME",
            100000,
            "pradeep@gmail.com");

        service.processEmployee(emp, "EMAIL");
    }
}

Without_SOLID.main([]);
