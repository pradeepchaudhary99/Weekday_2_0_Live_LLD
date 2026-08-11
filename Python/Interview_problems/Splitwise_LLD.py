"""
SplitWise System /App
Functional Requirements:
    Create Users
    Create Groups
    Add Expenses in the system
    Split Types
        - Equal Split
        - Percentage Split
    Maintain balances
        Balance[A][B] --> status of B in A's Balance sheet
    Show balances of a User

    should  support payments
Non-Functional
1. Thread-Safety
2. Consistency across the balance sheet (ACID Compliance)
3. Extensibility to support multiple types of splits/expense


----- OUT of Scope -------
    Simplification feature of splitwise : Graph Algorithm using Priority Queue
    Settle

Core Entities:

User
Groups
Expense
SplitStrategy(Interface)
EqualSplit
PercentageSplit

SplitWiseService

BalanceManagerService
"""

import threading
from abc import ABC, abstractmethod
from typing import Dict, List


class User:
    def __init__(self, id: str, name: str):
        self.id = id
        self.name = name


class Split:
    def __init__(self, paid_by: User, paid_for: User, split_amount: float, expense_id: str):
        self.paid_by = paid_by
        self.paid_for = paid_for
        self.split_amount = split_amount
        self.expense_id = expense_id


class SplitStrategy(ABC):
    @abstractmethod
    def generate_split(self, expense: "Expense") -> List[Split]:
        raise NotImplementedError


class Expense:
    def __init__(self, id: str, paid_by: User, amount: float,
                 users_participated: List[User], strategy: SplitStrategy):
        self.id = id
        self.paid_by = paid_by
        self.amount = amount
        self.users_participated = users_participated
        self.strategy = strategy
        self.splits: List[Split] = []

    def calculate_share(self) -> List[Split]:
        self.splits = self.strategy.generate_split(self)
        return self.splits


# Splits the amount equally among every participant except the payer.
class EqualSplit(SplitStrategy):
    def generate_split(self, expense: Expense) -> List[Split]:
        splits: List[Split] = []
        if not expense.users_participated:
            return splits
        share = expense.amount / len(expense.users_participated)
        for user in expense.users_participated:
            if user.id == expense.paid_by.id:
                continue
            splits.append(Split(expense.paid_by, user, share, expense.id))
        return splits


# Splits the amount according to a percentage assigned to each participant.
class PercentageSplit(SplitStrategy):
    def __init__(self, percentages: Dict[str, float]):
        self.percentages = percentages

    def generate_split(self, expense: Expense) -> List[Split]:
        splits: List[Split] = []
        for user in expense.users_participated:
            if user.id == expense.paid_by.id:
                continue
            percentage = self.percentages.get(user.id, 0.0)
            splits.append(Split(expense.paid_by, user, expense.amount * percentage / 100.0, expense.id))
        return splits


class Group:
    def __init__(self, id: str, name: str):
        self.id = id
        self.name = name
        self.users: List[User] = []
        self.expenses: List[Expense] = []


# balances[a][b] is what user b owes user a.
class BalanceManagerService:
    def __init__(self):
        self._balances: Dict[str, Dict[str, float]] = {}
        self._lock = threading.Lock()

    def _ensure_user(self, user_id: str) -> None:
        self._balances.setdefault(user_id, {})

    def update_balance_sheet(self, expense: Expense, splits: List[Split]) -> None:
        with self._lock:
            for split in splits:
                payer_id = split.paid_by.id
                debtor_id = split.paid_for.id
                self._ensure_user(payer_id)
                self._ensure_user(debtor_id)

                self._balances[payer_id][debtor_id] = self._balances[payer_id].get(debtor_id, 0.0) + split.split_amount
                self._balances[debtor_id][payer_id] = self._balances[debtor_id].get(payer_id, 0.0) - split.split_amount

    def get_balance(self, user_id: str) -> Dict[str, float]:
        with self._lock:
            return dict(self._balances.get(user_id, {}))


class SplitWiseService:
    def __init__(self):
        self._users_repository: Dict[str, User] = {}
        self._group_repository: Dict[str, Group] = {}
        self._balance_manager_service = BalanceManagerService()

    def create_user(self, user_id: str, name: str) -> User:
        user = User(user_id, name)
        self._users_repository[user_id] = user
        return user

    def create_group(self, group_id: str, name: str, users: List[User]) -> Group:
        group = Group(group_id, name)
        group.users = users
        self._group_repository[group_id] = group
        return group

    def add_expense(self, expense_id: str, paid_by: User, amount: float,
                     users_participated: List[User], strategy: SplitStrategy) -> Expense:
        expense = Expense(expense_id, paid_by, amount, users_participated, strategy)
        splits = expense.calculate_share()
        self._balance_manager_service.update_balance_sheet(expense, splits)
        return expense

    def show_balance(self, user: User) -> None:
        balances = self._balance_manager_service.get_balance(user.id)
        any_non_zero = False
        for other_id, amount in balances.items():
            if amount == 0:
                continue
            any_non_zero = True
            if amount > 0:
                print(f"{other_id} owes {user.name} {amount:.2f}")
            else:
                print(f"{user.name} owes {other_id} {-amount:.2f}")
        if not any_non_zero:
            print(f"No pending balances for {user.name}")


def main() -> None:
    service = SplitWiseService()

    alice = service.create_user("u1", "Alice")
    bob = service.create_user("u2", "Bob")
    carol = service.create_user("u3", "Carol")

    service.create_group("g1", "Goa Trip", [alice, bob, carol])

    print("Equal split: Alice pays 300 for all three")
    service.add_expense("e1", alice, 300.0, [alice, bob, carol], EqualSplit())

    print("Percentage split: Bob pays 200, Alice 30%, Carol 70%")
    percentages = {"u1": 30.0, "u3": 70.0}
    service.add_expense("e2", bob, 200.0, [alice, bob, carol], PercentageSplit(percentages))

    for user in [alice, bob, carol]:
        print(f"\nBalances for {user.name}:")
        service.show_balance(user)


if __name__ == "__main__":
    main()
