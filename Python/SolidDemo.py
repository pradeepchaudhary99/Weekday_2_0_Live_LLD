from abc import ABC, abstractmethod


# =======================
# 1. SINGLE RESPONSIBILITY PRINCIPLE (SRP)
# =======================

# BAD: One class doing multiple things
class UserBad:
    def __init__(self, name: str = ""):
        self.name = name

    def save_to_db(self) -> None:
        print("Saving user to DB")

    def send_email(self) -> None:
        print("Sending email")


# GOOD: Separate responsibilities
class User:
    def __init__(self, name: str = ""):
        self.name = name


class UserRepository:
    def save(self, user: User) -> None:
        print("Saving user to DB")


class EmailService:
    def send_email(self, user: User) -> None:
        print("Sending email")


# =======================
# 2. OPEN CLOSED PRINCIPLE (OCP)
# =======================

# BAD: Need to modify class for new types
class DiscountCalculatorBad:
    def calculate(self, type_: str) -> float:
        if type_ == "NEW":
            return 10
        elif type_ == "PREMIUM":
            return 20
        elif type_ == "DIWALI":
            return 30
        return 0


# GOOD: Extend without modifying
class DiscountStrategy(ABC):
    @abstractmethod
    def calculate(self) -> float:
        pass


class NewCustomerDiscount(DiscountStrategy):
    def calculate(self) -> float:
        return 10


class PremiumCustomerDiscount(DiscountStrategy):
    def calculate(self) -> float:
        return 20


class DiscountCalculator:
    def calculate(self, strategy: DiscountStrategy) -> float:
        return strategy.calculate()


# =======================
# 3. LISKOV SUBSTITUTION PRINCIPLE (LSP)
# =======================

# BAD: Violates substitution
class BirdBad:
    def fly(self) -> None:
        print("Flying")


class PenguinBad(BirdBad):
    def fly(self) -> None:
        raise NotImplementedError("Can't fly")


# GOOD: Segregate flying behavior into a separate interface
class Bird(ABC):
    pass


class FlyingBird(Bird, ABC):
    @abstractmethod
    def fly(self) -> None:
        pass


class Sparrow(FlyingBird):
    def fly(self) -> None:
        print("Flying")


# =======================
# 4. INTERFACE SEGREGATION PRINCIPLE (ISP)
# =======================

# BAD: Fat interface
class WorkerBad(ABC):
    @abstractmethod
    def work(self) -> None:
        pass

    @abstractmethod
    def eat(self) -> None:
        pass


class RobotBad(WorkerBad):
    def work(self) -> None:
        print("Working")

    def eat(self) -> None:
        raise NotImplementedError("Robot doesn't eat")


# GOOD: Split interfaces
class Workable(ABC):
    @abstractmethod
    def work(self) -> None:
        pass


class Eatable(ABC):
    @abstractmethod
    def eat(self) -> None:
        pass


class Human(Workable, Eatable):
    def work(self) -> None:
        print("Working")

    def eat(self) -> None:
        print("Eating")


class Robot(Workable):
    def work(self) -> None:
        print("Working")


# =======================
# 5. DEPENDENCY INVERSION PRINCIPLE (DIP)
# =======================

# BAD: High-level depends on low-level
class MySQLDatabase:
    def connect(self) -> None:
        print("Connecting to MySQL")


class ApplicationBad:
    def __init__(self):
        self.db = MySQLDatabase()

    def start(self) -> None:
        self.db.connect()


# GOOD: Depend on abstraction
class Database(ABC):
    @abstractmethod
    def connect(self) -> None:
        pass


class MySQL(Database):
    def connect(self) -> None:
        print("Connecting to MySQL")


class PostgreSQL(Database):
    def connect(self) -> None:
        print("Connecting to PostgreSQL")


class Application:
    def __init__(self, db: Database):
        self.db = db

    def start(self) -> None:
        self.db.connect()


# =======================
# MAIN (TESTING)
# =======================
if __name__ == "__main__":
    # SRP
    user = User()
    UserRepository().save(user)
    EmailService().send_email(user)

    # OCP
    calc = DiscountCalculator()
    print(calc.calculate(PremiumCustomerDiscount()))

    # LSP
    bird: FlyingBird = Sparrow()
    bird.fly()

    # ISP
    robot: Workable = Robot()
    robot.work()

    # DIP
    app = Application(MySQL())
    app.start()
