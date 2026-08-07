"""
================================================================================
LLD: Splitwise (Expense Sharing)
================================================================================

Problem: Design an expense-sharing service where a group of users can log
shared expenses and, at any point, ask "who owes whom, and how much?".

Functional Requirements:
    1. Register users.
    2. Add an expense: one user pays a total amount on behalf of a set of
       participants.
    3. Support more than one way to divide an expense (equal, exact amounts,
       percentages) behind a common interface, so a new split type can be
       added without touching call sites.
    4. At any time, produce the net amount every user owes every other user.

Non-Functional Requirements:
    1. Correctness: exact/percentage splits must be validated against the
       total (exact amounts must sum to the total, percentages to 100),
       otherwise silent rounding bugs corrupt the ledger.
    2. Minimal bookkeeping: if Bob owes Alice AND Alice separately owes Bob,
       the ledger nets those two entries into a single directional debt
       instead of tracking both.
    3. Extensibility: adding a new split strategy (e.g. by shares) should
       mean adding a new class, not editing existing ones.

Design (Strategy pattern):
    SplitStrategy is the abstraction every split type implements. It turns
    "$total among these participants" into a list of per-user Splits.
    EqualSplitStrategy divides evenly (giving the remainder's leftover cents
    to the first participants so the split always sums exactly back to the
    total). ExactSplitStrategy and PercentSplitStrategy both validate their
    input sums before returning a single split -- this is what a naive stub
    gets wrong: trusting caller-supplied exact amounts/percentages without
    checking they add up, which lets the ledger silently drift.

    BalanceSheet stores a net, directional debt per unordered pair of users
    (debts[X][Y] = amount X owes Y). Every new debt is first offset against
    any existing debt in the opposite direction, so the sheet always holds
    the minimal representation of who owes whom.

Core Entities:
    User
    Split / SplitStrategy (Equal / Exact / Percent)
    Expense
    BalanceSheet
    SplitwiseService
================================================================================
"""

from abc import ABC, abstractmethod
from enum import Enum, auto
from typing import Dict, List, Optional


class User:
    def __init__(self, user_id: str, name: str):
        self.id = user_id
        self.name = name


class Split:
    def __init__(self, user: User, amount: float):
        self.user = user
        self.amount = amount


class SplitStrategy(ABC):
    @abstractmethod
    def calculate(self, total_amount: float, participants: List[User],
                  values: Dict[str, float]) -> List[Split]:
        raise NotImplementedError


class EqualSplitStrategy(SplitStrategy):
    def calculate(self, total_amount: float, participants: List[User],
                  values: Dict[str, float]) -> List[Split]:
        splits = []
        n = len(participants)
        # Round each share to cents, then hand the leftover cents (caused by
        # rounding) to the first participants so the splits sum exactly to
        # total_amount instead of drifting by a cent or two.
        total_cents = round(total_amount * 100)
        base_share_cents = total_cents // n
        remainder_cents = total_cents % n
        for i, user in enumerate(participants):
            share_cents = base_share_cents + (1 if i < remainder_cents else 0)
            splits.append(Split(user, share_cents / 100.0))
        return splits


class ExactSplitStrategy(SplitStrategy):
    def calculate(self, total_amount: float, participants: List[User],
                  values: Dict[str, float]) -> List[Split]:
        splits = []
        total = 0.0
        for user in participants:
            amount = values.get(user.id)
            if amount is None:
                raise ValueError(f"Missing exact amount for user {user.id}")
            total += amount
            splits.append(Split(user, amount))
        if abs(total - total_amount) > 0.01:
            raise ValueError(
                f"Exact amounts ({total}) do not add up to the total ({total_amount})")
        return splits


class PercentSplitStrategy(SplitStrategy):
    def calculate(self, total_amount: float, participants: List[User],
                  values: Dict[str, float]) -> List[Split]:
        splits = []
        percent_sum = 0.0
        for user in participants:
            percent = values.get(user.id)
            if percent is None:
                raise ValueError(f"Missing percentage for user {user.id}")
            percent_sum += percent
            splits.append(Split(user, total_amount * percent / 100.0))
        if abs(percent_sum - 100.0) > 0.01:
            raise ValueError(f"Percentages ({percent_sum}) do not add up to 100")
        return splits


class SplitType(Enum):
    EQUAL = auto()
    EXACT = auto()
    PERCENT = auto()


class Expense:
    def __init__(self, description: str, amount: float, paid_by: User, splits: List[Split]):
        self.description = description
        self.amount = amount
        self.paid_by = paid_by
        self.splits = splits


class BalanceSheet:
    """Tracks the net, directional debt between every pair of users:
    debts[X][Y] = amount X still owes Y. Only one of debts[X][Y] /
    debts[Y][X] is ever non-zero at a time."""

    def __init__(self):
        self._debts: Dict[str, Dict[str, float]] = {}

    def add_debt(self, debtor_id: str, creditor_id: str, amount: float) -> None:
        if debtor_id == creditor_id or amount <= 0:
            return
        owed_back = self._get_debt(creditor_id, debtor_id)
        if owed_back >= amount:
            self._set_debt(creditor_id, debtor_id, owed_back - amount)
        else:
            self._set_debt(creditor_id, debtor_id, 0.0)
            self._set_debt(debtor_id, creditor_id,
                            self._get_debt(debtor_id, creditor_id) + (amount - owed_back))

    def _get_debt(self, debtor_id: str, creditor_id: str) -> float:
        return self._debts.get(debtor_id, {}).get(creditor_id, 0.0)

    def _set_debt(self, debtor_id: str, creditor_id: str, amount: float) -> None:
        self._debts.setdefault(debtor_id, {})[creditor_id] = amount

    def print_balances(self) -> None:
        any_debt = False
        for debtor_id in sorted(self._debts.keys()):
            for creditor_id in sorted(self._debts[debtor_id].keys()):
                amount = self._debts[debtor_id][creditor_id]
                if amount > 0.005:
                    print(f"  {debtor_id} owes {creditor_id}: {amount:.2f}")
                    any_debt = True
        if not any_debt:
            print("  Everyone is settled up.")


class SplitwiseService:
    def __init__(self):
        self._users: Dict[str, User] = {}
        self._balance_sheet = BalanceSheet()
        self._strategies: Dict[SplitType, SplitStrategy] = {
            SplitType.EQUAL: EqualSplitStrategy(),
            SplitType.EXACT: ExactSplitStrategy(),
            SplitType.PERCENT: PercentSplitStrategy(),
        }

    def add_user(self, user: User) -> None:
        self._users[user.id] = user

    def add_expense(self, description: str, amount: float, paid_by_user_id: str,
                     participant_ids: List[str], split_type: SplitType,
                     values: Optional[Dict[str, float]] = None) -> Expense:
        values = values or {}
        paid_by = self._users[paid_by_user_id]
        participants = [self._users[pid] for pid in participant_ids]

        splits = self._strategies[split_type].calculate(amount, participants, values)

        for split in splits:
            if split.user.id != paid_by_user_id:
                self._balance_sheet.add_debt(split.user.id, paid_by_user_id, split.amount)

        return Expense(description, amount, paid_by, splits)

    def print_balances(self) -> None:
        self._balance_sheet.print_balances()


def main() -> None:
    service = SplitwiseService()
    service.add_user(User("alice", "Alice"))
    service.add_user(User("bob", "Bob"))
    service.add_user(User("charlie", "Charlie"))
    service.add_user(User("dave", "Dave"))

    print("Alice pays 400.00, split EQUALLY among alice, bob, charlie, dave")
    service.add_expense("Dinner", 400.00, "alice",
                         ["alice", "bob", "charlie", "dave"], SplitType.EQUAL)
    service.print_balances()

    print("\nBob pays 300.00, split EXACTLY: alice=100, bob=50, charlie=150")
    service.add_expense("Groceries", 300.00, "bob",
                         ["alice", "bob", "charlie"], SplitType.EXACT,
                         {"alice": 100.0, "bob": 50.0, "charlie": 150.0})
    service.print_balances()

    print("\nCharlie pays 1000.00, split by PERCENT: alice=20%, bob=30%, charlie=50%")
    service.add_expense("Trip", 1000.00, "charlie",
                         ["alice", "bob", "charlie"], SplitType.PERCENT,
                         {"alice": 20.0, "bob": 30.0, "charlie": 50.0})
    service.print_balances()

    print("\nAttempting an EXACT split that doesn't add up to the total:")
    try:
        service.add_expense("Bad split", 100.00, "dave",
                             ["alice", "dave"], SplitType.EXACT,
                             {"alice": 40.0, "dave": 50.0})
    except ValueError as e:
        print(f"  Rejected: {e}")


if __name__ == "__main__":
    main()
