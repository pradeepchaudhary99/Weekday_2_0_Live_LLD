import java.util.*;

public class Without_SOLID {

    public static void main(String[] args) {

        EmployeeService service = new EmployeeService();

        Employee emp = new Employee(
                101,
                "Pradeep",
                "FULL_TIME",
                100000,
                "pradeep@gmail.com");

        service.processEmployee(emp, "EMAIL");
    }
}

class Employee {

    private int id;
    private String name;
    private String type;
    private double salary;
    private String email;

    public Employee(int id, String name, String type, double salary, String email) {
        this.id = id;
        this.name = name;
        this.type = type;
        this.salary = salary;
        this.email = email;
    }

    public int getId() {
        return id;
    }

    public String getName() {
        return name;
    }

    public String getType() {
        return type;
    }

    public double getSalary() {
        return salary;
    }

    public String getEmail() {
        return email;
    }
}

class EmployeeService {

    public void processEmployee(Employee employee, String notificationType) {

        // =====================
        // Validation
        // =====================

        if (employee.getName() == null || employee.getName().isEmpty()) {
            throw new RuntimeException("Invalid Name");
        }

        if (!employee.getEmail().contains("@")) {
            throw new RuntimeException("Invalid Email");
        }

        // =====================
        // Salary Calculation
        // =====================

        double finalSalary = 0;

        if (employee.getType().equals("FULL_TIME")) {
            finalSalary = employee.getSalary();
        }
        else if (employee.getType().equals("INTERN")) {
            finalSalary = employee.getSalary() * 0.5;
        }
        else if (employee.getType().equals("CONTRACT")) {
            finalSalary = employee.getSalary() * 1.2;
        }
        else {
            throw new RuntimeException("Unknown Employee Type");
        }

        System.out.println("Salary : " + finalSalary);

        // =====================
        // Database
        // =====================

        System.out.println("Connecting to MySQL...");
        System.out.println("Saving Employee...");
        System.out.println("Employee Saved");

        // =====================
        // Notification
        // =====================

        if (notificationType.equals("EMAIL")) {
            System.out.println("Sending Email to " + employee.getEmail());
        }
        else if (notificationType.equals("SMS")) {
            System.out.println("Sending SMS");
        }
        else if (notificationType.equals("WHATSAPP")) {
            System.out.println("Sending WhatsApp Message");
        }

        // =====================
        // Audit
        // =====================

        System.out.println("Writing Audit Log");

        // =====================
        // Analytics
        // =====================

        System.out.println("Publishing Analytics Event");

        // =====================
        // Invoice
        // =====================

        System.out.println("Generating Salary Slip");

        // =====================
        // Cloud Upload
        // =====================

        System.out.println("Uploading Salary Slip to AWS S3");

        // =====================
        // Cache
        // =====================

        System.out.println("Updating Redis Cache");

        // =====================
        // Report
        // =====================

        System.out.println("Generating Monthly Report");

        System.out.println("Employee Processing Completed");
    }
}