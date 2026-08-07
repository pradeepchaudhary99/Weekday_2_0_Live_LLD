/*
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
*/

class User {
    constructor(id, name) {
        this.id = id;
        this.name = name;
    }
}

class Split {
    constructor(user, amount) {
        this.user = user;
        this.amount = amount;
    }
}

class SplitStrategy {
    calculate(totalAmount, participants, values) {
        throw new Error("calculate() must be implemented");
    }
}

class EqualSplitStrategy extends SplitStrategy {
    calculate(totalAmount, participants) {
        const splits = [];
        const n = participants.length;
        // Round each share to cents, then hand the leftover cents (caused by
        // rounding) to the first participants so the splits sum exactly to
        // totalAmount instead of drifting by a cent or two.
        const totalCents = Math.round(totalAmount * 100);
        const baseShareCents = Math.floor(totalCents / n);
        const remainderCents = totalCents % n;
        for (let i = 0; i < n; i++) {
            const shareCents = baseShareCents + (i < remainderCents ? 1 : 0);
            splits.push(new Split(participants[i], shareCents / 100.0));
        }
        return splits;
    }
}

class ExactSplitStrategy extends SplitStrategy {
    calculate(totalAmount, participants, values) {
        const splits = [];
        let sum = 0;
        for (const user of participants) {
            const amount = values[user.id];
            if (amount === undefined) {
                throw new Error(`Missing exact amount for user ${user.id}`);
            }
            sum += amount;
            splits.push(new Split(user, amount));
        }
        if (Math.abs(sum - totalAmount) > 0.01) {
            throw new Error(`Exact amounts (${sum}) do not add up to the total (${totalAmount})`);
        }
        return splits;
    }
}

class PercentSplitStrategy extends SplitStrategy {
    calculate(totalAmount, participants, values) {
        const splits = [];
        let percentSum = 0;
        for (const user of participants) {
            const percent = values[user.id];
            if (percent === undefined) {
                throw new Error(`Missing percentage for user ${user.id}`);
            }
            percentSum += percent;
            splits.push(new Split(user, (totalAmount * percent) / 100.0));
        }
        if (Math.abs(percentSum - 100.0) > 0.01) {
            throw new Error(`Percentages (${percentSum}) do not add up to 100`);
        }
        return splits;
    }
}

const SplitType = Object.freeze({
    EQUAL: "EQUAL",
    EXACT: "EXACT",
    PERCENT: "PERCENT",
});

class Expense {
    constructor(description, amount, paidBy, splits) {
        this.description = description;
        this.amount = amount;
        this.paidBy = paidBy;
        this.splits = splits;
    }
}

// Tracks the net, directional debt between every pair of users:
// debts[X][Y] = amount X still owes Y. Only one of debts[X][Y] / debts[Y][X]
// is ever non-zero at a time.
class BalanceSheet {
    constructor() {
        this._debts = new Map();
    }

    addDebt(debtorId, creditorId, amount) {
        if (debtorId === creditorId || amount <= 0) {
            return;
        }
        const owedBack = this._getDebt(creditorId, debtorId);
        if (owedBack >= amount) {
            this._setDebt(creditorId, debtorId, owedBack - amount);
        } else {
            this._setDebt(creditorId, debtorId, 0);
            this._setDebt(debtorId, creditorId, this._getDebt(debtorId, creditorId) + (amount - owedBack));
        }
    }

    _getDebt(debtorId, creditorId) {
        const inner = this._debts.get(debtorId);
        return inner ? inner.get(creditorId) || 0 : 0;
    }

    _setDebt(debtorId, creditorId, amount) {
        if (!this._debts.has(debtorId)) {
            this._debts.set(debtorId, new Map());
        }
        this._debts.get(debtorId).set(creditorId, amount);
    }

    printBalances() {
        let any = false;
        const debtorIds = [...this._debts.keys()].sort();
        for (const debtorId of debtorIds) {
            const creditorIds = [...this._debts.get(debtorId).keys()].sort();
            for (const creditorId of creditorIds) {
                const amount = this._debts.get(debtorId).get(creditorId);
                if (amount > 0.005) {
                    console.log(`  ${debtorId} owes ${creditorId}: ${amount.toFixed(2)}`);
                    any = true;
                }
            }
        }
        if (!any) {
            console.log("  Everyone is settled up.");
        }
    }
}

class SplitwiseService {
    constructor() {
        this._users = new Map();
        this._balanceSheet = new BalanceSheet();
        this._strategies = {
            [SplitType.EQUAL]: new EqualSplitStrategy(),
            [SplitType.EXACT]: new ExactSplitStrategy(),
            [SplitType.PERCENT]: new PercentSplitStrategy(),
        };
    }

    addUser(user) {
        this._users.set(user.id, user);
    }

    addExpense(description, amount, paidByUserId, participantIds, splitType, values = {}) {
        const paidBy = this._users.get(paidByUserId);
        const participants = participantIds.map((id) => this._users.get(id));

        const splits = this._strategies[splitType].calculate(amount, participants, values);

        for (const split of splits) {
            if (split.user.id !== paidByUserId) {
                this._balanceSheet.addDebt(split.user.id, paidByUserId, split.amount);
            }
        }

        return new Expense(description, amount, paidBy, splits);
    }

    printBalances() {
        this._balanceSheet.printBalances();
    }
}

function main() {
    const service = new SplitwiseService();
    service.addUser(new User("alice", "Alice"));
    service.addUser(new User("bob", "Bob"));
    service.addUser(new User("charlie", "Charlie"));
    service.addUser(new User("dave", "Dave"));

    console.log("Alice pays 400.00, split EQUALLY among alice, bob, charlie, dave");
    service.addExpense("Dinner", 400.0, "alice", ["alice", "bob", "charlie", "dave"], SplitType.EQUAL);
    service.printBalances();

    console.log("\nBob pays 300.00, split EXACTLY: alice=100, bob=50, charlie=150");
    service.addExpense("Groceries", 300.0, "bob", ["alice", "bob", "charlie"], SplitType.EXACT, {
        alice: 100.0,
        bob: 50.0,
        charlie: 150.0,
    });
    service.printBalances();

    console.log("\nCharlie pays 1000.00, split by PERCENT: alice=20%, bob=30%, charlie=50%");
    service.addExpense("Trip", 1000.0, "charlie", ["alice", "bob", "charlie"], SplitType.PERCENT, {
        alice: 20.0,
        bob: 30.0,
        charlie: 50.0,
    });
    service.printBalances();

    console.log("\nAttempting an EXACT split that doesn't add up to the total:");
    try {
        service.addExpense("Bad split", 100.0, "dave", ["alice", "dave"], SplitType.EXACT, {
            alice: 40.0,
            dave: 50.0,
        });
    } catch (e) {
        console.log(`  Rejected: ${e.message}`);
    }
}

main();
