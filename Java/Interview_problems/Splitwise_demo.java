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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;

class User {
    final String id;
    final String name;

    User(String id, String name) {
        this.id = id;
        this.name = name;
    }
}

class Split {
    final User user;
    final double amount;

    Split(User user, double amount) {
        this.user = user;
        this.amount = amount;
    }
}

interface SplitStrategy {
    List<Split> calculate(double totalAmount, List<User> participants, Map<String, Double> values);
}

class EqualSplitStrategy implements SplitStrategy {
    @Override
    public List<Split> calculate(double totalAmount, List<User> participants, Map<String, Double> values) {
        List<Split> splits = new ArrayList<>();
        int n = participants.size();
        // Round each share to cents, then hand the leftover cents (caused by
        // rounding) to the first participants so the splits sum exactly to
        // totalAmount instead of drifting by a cent or two.
        long totalCents = Math.round(totalAmount * 100);
        long baseShareCents = totalCents / n;
        long remainderCents = totalCents % n;
        for (int i = 0; i < n; i++) {
            long shareCents = baseShareCents + (i < remainderCents ? 1 : 0);
            splits.add(new Split(participants.get(i), shareCents / 100.0));
        }
        return splits;
    }
}

class ExactSplitStrategy implements SplitStrategy {
    @Override
    public List<Split> calculate(double totalAmount, List<User> participants, Map<String, Double> values) {
        List<Split> splits = new ArrayList<>();
        double sum = 0;
        for (User user : participants) {
            Double amount = values.get(user.id);
            if (amount == null) {
                throw new IllegalArgumentException("Missing exact amount for user " + user.id);
            }
            sum += amount;
            splits.add(new Split(user, amount));
        }
        if (Math.abs(sum - totalAmount) > 0.01) {
            throw new IllegalArgumentException(
                    "Exact amounts (" + sum + ") do not add up to the total (" + totalAmount + ")");
        }
        return splits;
    }
}

class PercentSplitStrategy implements SplitStrategy {
    @Override
    public List<Split> calculate(double totalAmount, List<User> participants, Map<String, Double> values) {
        List<Split> splits = new ArrayList<>();
        double percentSum = 0;
        for (User user : participants) {
            Double percent = values.get(user.id);
            if (percent == null) {
                throw new IllegalArgumentException("Missing percentage for user " + user.id);
            }
            percentSum += percent;
            splits.add(new Split(user, totalAmount * percent / 100.0));
        }
        if (Math.abs(percentSum - 100.0) > 0.01) {
            throw new IllegalArgumentException("Percentages (" + percentSum + ") do not add up to 100");
        }
        return splits;
    }
}

enum SplitType {
    EQUAL, EXACT, PERCENT
}

class Expense {
    final String description;
    final double amount;
    final User paidBy;
    final List<Split> splits;

    Expense(String description, double amount, User paidBy, List<Split> splits) {
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
    private final Map<String, Map<String, Double>> debts = new TreeMap<>();

    void addDebt(String debtorId, String creditorId, double amount) {
        if (debtorId.equals(creditorId) || amount <= 0) {
            return;
        }
        double owedBack = getDebt(creditorId, debtorId);
        if (owedBack >= amount) {
            setDebt(creditorId, debtorId, owedBack - amount);
        } else {
            setDebt(creditorId, debtorId, 0);
            setDebt(debtorId, creditorId, getDebt(debtorId, creditorId) + (amount - owedBack));
        }
    }

    private double getDebt(String debtorId, String creditorId) {
        return debts.getOrDefault(debtorId, Map.of()).getOrDefault(creditorId, 0.0);
    }

    private void setDebt(String debtorId, String creditorId, double amount) {
        debts.computeIfAbsent(debtorId, k -> new TreeMap<>()).put(creditorId, amount);
    }

    void printBalances() {
        boolean any = false;
        for (Map.Entry<String, Map<String, Double>> entry : debts.entrySet()) {
            for (Map.Entry<String, Double> owed : entry.getValue().entrySet()) {
                if (owed.getValue() > 0.005) {
                    System.out.printf("  %s owes %s: %.2f%n", entry.getKey(), owed.getKey(), owed.getValue());
                    any = true;
                }
            }
        }
        if (!any) {
            System.out.println("  Everyone is settled up.");
        }
    }
}

class SplitwiseService {
    private final Map<String, User> users = new HashMap<>();
    private final BalanceSheet balanceSheet = new BalanceSheet();
    private final Map<SplitType, SplitStrategy> strategies = Map.of(
            SplitType.EQUAL, new EqualSplitStrategy(),
            SplitType.EXACT, new ExactSplitStrategy(),
            SplitType.PERCENT, new PercentSplitStrategy());

    void addUser(User user) {
        users.put(user.id, user);
    }

    Expense addExpense(String description, double amount, String paidByUserId,
                        List<String> participantIds, SplitType type, Map<String, Double> values) {
        User paidBy = users.get(paidByUserId);
        List<User> participants = new ArrayList<>();
        for (String id : participantIds) {
            participants.add(users.get(id));
        }

        List<Split> splits = strategies.get(type).calculate(amount, participants, values);

        for (Split split : splits) {
            if (!split.user.id.equals(paidByUserId)) {
                balanceSheet.addDebt(split.user.id, paidByUserId, split.amount);
            }
        }

        return new Expense(description, amount, paidBy, splits);
    }

    void printBalances() {
        balanceSheet.printBalances();
    }
}

public class Splitwise_demo {
    public static void main(String[] args) {
        SplitwiseService service = new SplitwiseService();
        service.addUser(new User("alice", "Alice"));
        service.addUser(new User("bob", "Bob"));
        service.addUser(new User("charlie", "Charlie"));
        service.addUser(new User("dave", "Dave"));

        System.out.println("Alice pays 400.00, split EQUALLY among alice, bob, charlie, dave");
        service.addExpense("Dinner", 400.00, "alice",
                List.of("alice", "bob", "charlie", "dave"), SplitType.EQUAL, Map.of());
        service.printBalances();

        System.out.println("\nBob pays 300.00, split EXACTLY: alice=100, bob=50, charlie=150");
        service.addExpense("Groceries", 300.00, "bob",
                List.of("alice", "bob", "charlie"), SplitType.EXACT,
                Map.of("alice", 100.0, "bob", 50.0, "charlie", 150.0));
        service.printBalances();

        System.out.println("\nCharlie pays 1000.00, split by PERCENT: alice=20%, bob=30%, charlie=50%");
        service.addExpense("Trip", 1000.00, "charlie",
                List.of("alice", "bob", "charlie"), SplitType.PERCENT,
                Map.of("alice", 20.0, "bob", 30.0, "charlie", 50.0));
        service.printBalances();

        System.out.println("\nAttempting an EXACT split that doesn't add up to the total:");
        try {
            service.addExpense("Bad split", 100.00, "dave",
                    List.of("alice", "dave"), SplitType.EXACT,
                    Map.of("alice", 40.0, "dave", 50.0));
        } catch (IllegalArgumentException e) {
            System.out.println("  Rejected: " + e.getMessage());
        }
    }
}
