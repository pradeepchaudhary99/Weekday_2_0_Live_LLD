class Employee:
    def __init__(self, id: int, name: str, type: str, salary: float, email: str):
        self._id = id
        self._name = name
        self._type = type
        self._salary = salary
        self._email = email

    def get_id(self) -> int:
        return self._id

    def get_name(self) -> str:
        return self._name

    def get_type(self) -> str:
        return self._type

    def get_salary(self) -> float:
        return self._salary

    def get_email(self) -> str:
        return self._email


class EmployeeService:
    def process_employee(self, employee: Employee, notification_type: str) -> None:
        # =====================
        # Validation
        # =====================
        if not employee.get_name():
            raise RuntimeError("Invalid Name")
        if "@" not in employee.get_email():
            raise RuntimeError("Invalid Email")

        # =====================
        # Salary Calculation
        # =====================
        emp_type = employee.get_type()
        if emp_type == "FULL_TIME":
            final_salary = employee.get_salary()
        elif emp_type == "INTERN":
            final_salary = employee.get_salary() * 0.5
        elif emp_type == "CONTRACT":
            final_salary = employee.get_salary() * 1.2
        else:
            raise RuntimeError("Unknown Employee Type")

        print(f"Salary : {final_salary}")

        # =====================
        # Database
        # =====================
        print("Connecting to MySQL...")
        print("Saving Employee...")
        print("Employee Saved")

        # =====================
        # Notification
        # =====================
        if notification_type == "EMAIL":
            print(f"Sending Email to {employee.get_email()}")
        elif notification_type == "SMS":
            print("Sending SMS")
        elif notification_type == "WHATSAPP":
            print("Sending WhatsApp Message")

        # =====================
        # Audit
        # =====================
        print("Writing Audit Log")

        # =====================
        # Analytics
        # =====================
        print("Publishing Analytics Event")

        # =====================
        # Invoice
        # =====================
        print("Generating Salary Slip")

        # =====================
        # Cloud Upload
        # =====================
        print("Uploading Salary Slip to AWS S3")

        # =====================
        # Cache
        # =====================
        print("Updating Redis Cache")

        # =====================
        # Report
        # =====================
        print("Generating Monthly Report")

        print("Employee Processing Completed")


if __name__ == "__main__":
    service = EmployeeService()
    emp = Employee(101, "Pradeep", "FULL_TIME", 100000, "pradeep@gmail.com")
    service.process_employee(emp, "EMAIL")
