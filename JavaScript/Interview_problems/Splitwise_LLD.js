/*
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
*/

class User {
    constructor(id, name) {
        this.id = id;
        this.name = name;
    }
}

class Split {
    constructor(paidBy, paidFor, splitAmount, expenseId) {
        this.paidBy = paidBy;
        this.paidFor = paidFor;
        this.splitAmount = splitAmount;
        this.expenseId = expenseId;
    }
}

class SplitStrategy {
    generateSplit(_expense) {
        throw new Error("generateSplit must be implemented by subclasses");
    }
}

class Expense {
    constructor(id, paidBy, amount, usersParticipated, strategy) {
        this.id = id;
        this.paidBy = paidBy;
        this.amount = amount;
        this.usersParticipated = usersParticipated;
        this.strategy = strategy;
        this.splits = [];
    }

    calculateShare() {
        this.splits = this.strategy.generateSplit(this);
        return this.splits;
    }
}

// Splits the amount equally among every participant except the payer.
class EqualSplit extends SplitStrategy {
    generateSplit(expense) {
        const splits = [];
        if (expense.usersParticipated.length === 0) {
            return splits;
        }
        const share = expense.amount / expense.usersParticipated.length;
        for (const user of expense.usersParticipated) {
            if (user.id === expense.paidBy.id) {
                continue;
            }
            splits.push(new Split(expense.paidBy, user, share, expense.id));
        }
        return splits;
    }
}

// Splits the amount according to a percentage assigned to each participant.
class PercentageSplit extends SplitStrategy {
    constructor(percentages) {
        super();
        this.percentages = percentages;
    }

    generateSplit(expense) {
        const splits = [];
        for (const user of expense.usersParticipated) {
            if (user.id === expense.paidBy.id) {
                continue;
            }
            const percentage = this.percentages[user.id] ?? 0.0;
            splits.push(new Split(expense.paidBy, user, (expense.amount * percentage) / 100.0, expense.id));
        }
        return splits;
    }
}

class Group {
    constructor(id, name) {
        this.id = id;
        this.name = name;
        this.users = [];
        this.expenses = [];
    }
}

// balances[a][b] is what user b owes user a.
class BalanceManagerService {
    constructor() {
        this._balances = new Map();
    }

    _ensureUser(userId) {
        if (!this._balances.has(userId)) {
            this._balances.set(userId, new Map());
        }
    }

    updateBalanceSheet(_expense, splits) {
        for (const split of splits) {
            const payerId = split.paidBy.id;
            const debtorId = split.paidFor.id;
            this._ensureUser(payerId);
            this._ensureUser(debtorId);

            const payerSheet = this._balances.get(payerId);
            const debtorSheet = this._balances.get(debtorId);
            payerSheet.set(debtorId, (payerSheet.get(debtorId) ?? 0.0) + split.splitAmount);
            debtorSheet.set(payerId, (debtorSheet.get(payerId) ?? 0.0) - split.splitAmount);
        }
    }

    getBalance(userId) {
        return new Map(this._balances.get(userId) ?? new Map());
    }
}

class SplitWiseService {
    constructor() {
        this._usersRepository = new Map();
        this._groupRepository = new Map();
        this._balanceManagerService = new BalanceManagerService();
    }

    createUser(userId, name) {
        const user = new User(userId, name);
        this._usersRepository.set(userId, user);
        return user;
    }

    createGroup(groupId, name, users) {
        const group = new Group(groupId, name);
        group.users = users;
        this._groupRepository.set(groupId, group);
        return group;
    }

    addExpense(expenseId, paidBy, amount, usersParticipated, strategy) {
        const expense = new Expense(expenseId, paidBy, amount, usersParticipated, strategy);
        const splits = expense.calculateShare();
        this._balanceManagerService.updateBalanceSheet(expense, splits);
        return expense;
    }

    showBalance(user) {
        const balances = this._balanceManagerService.getBalance(user.id);
        let anyNonZero = false;
        for (const [otherId, amount] of balances) {
            if (amount === 0) {
                continue;
            }
            anyNonZero = true;
            if (amount > 0) {
                console.log(`${otherId} owes ${user.name} ${amount.toFixed(2)}`);
            } else {
                console.log(`${user.name} owes ${otherId} ${(-amount).toFixed(2)}`);
            }
        }
        if (!anyNonZero) {
            console.log(`No pending balances for ${user.name}`);
        }
    }
}

function main() {
    const service = new SplitWiseService();

    const alice = service.createUser("u1", "Alice");
    const bob = service.createUser("u2", "Bob");
    const carol = service.createUser("u3", "Carol");

    service.createGroup("g1", "Goa Trip", [alice, bob, carol]);

    console.log("Equal split: Alice pays 300 for all three");
    service.addExpense("e1", alice, 300.0, [alice, bob, carol], new EqualSplit());

    console.log("Percentage split: Bob pays 200, Alice 30%, Carol 70%");
    const percentages = { u1: 30.0, u3: 70.0 };
    service.addExpense("e2", bob, 200.0, [alice, bob, carol], new PercentageSplit(percentages));

    for (const user of [alice, bob, carol]) {
        console.log(`\nBalances for ${user.name}:`);
        service.showBalance(user);
    }
}

main();
