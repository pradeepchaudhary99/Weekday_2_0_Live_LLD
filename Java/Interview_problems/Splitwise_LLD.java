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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class User {
    private final String id;
    private final String name;

    public User(String id, String name) {
        this.id = id;
        this.name = name;
    }

    public String getId() {
        return id;
    }

    public String getName() {
        return name;
    }
}

class Split {
    User paidBy;
    User paidFor;
    double splitAmount;
    String expenseId;

    public Split(User paidBy, User paidFor, double splitAmount, String expenseId) {
        this.paidBy = paidBy;
        this.paidFor = paidFor;
        this.splitAmount = splitAmount;
        this.expenseId = expenseId;
    }
}

interface SplitStrategy {
    List<Split> generateSplit(Expense expense);
}

class Expense {
    String id;
    User paidBy;
    double amount;
    List<User> usersParticipated;
    SplitStrategy strategy;
    List<Split> splits;

    public Expense(String id, User paidBy, double amount, List<User> usersParticipated, SplitStrategy strategy) {
        this.id = id;
        this.paidBy = paidBy;
        this.amount = amount;
        this.usersParticipated = usersParticipated;
        this.strategy = strategy;
    }

    public List<Split> calculateShare() {
        splits = strategy.generateSplit(this);
        return splits;
    }
}

// Splits the amount equally among every participant except the payer.
class EqualSplit implements SplitStrategy {
    @Override
    public List<Split> generateSplit(Expense expense) {
        List<Split> splits = new ArrayList<>();
        if (expense.usersParticipated.isEmpty()) {
            return splits;
        }
        double share = expense.amount / expense.usersParticipated.size();
        for (User user : expense.usersParticipated) {
            if (user.getId().equals(expense.paidBy.getId())) {
                continue;
            }
            splits.add(new Split(expense.paidBy, user, share, expense.id));
        }
        return splits;
    }
}

// Splits the amount according to a percentage assigned to each participant.
class PercentageSplit implements SplitStrategy {
    private final Map<String, Double> percentages;

    public PercentageSplit(Map<String, Double> percentages) {
        this.percentages = percentages;
    }

    @Override
    public List<Split> generateSplit(Expense expense) {
        List<Split> splits = new ArrayList<>();
        for (User user : expense.usersParticipated) {
            if (user.getId().equals(expense.paidBy.getId())) {
                continue;
            }
            double percentage = percentages.getOrDefault(user.getId(), 0.0);
            splits.add(new Split(expense.paidBy, user, expense.amount * percentage / 100.0, expense.id));
        }
        return splits;
    }
}

class Group {
    String id;
    String name;
    List<User> users = new ArrayList<>();
    List<Expense> expenses = new ArrayList<>();

    public Group(String id, String name) {
        this.id = id;
        this.name = name;
    }
}

// balances.get(a).get(b) is what user b owes user a.
class BalanceManagerService {
    private final Map<String, Map<String, Double>> balances = new HashMap<>();

    private void ensureUser(String userId) {
        balances.putIfAbsent(userId, new HashMap<>());
    }

    public synchronized void updateBalanceSheet(Expense expense, List<Split> splits) {
        for (Split split : splits) {
            String payerId = split.paidBy.getId();
            String debtorId = split.paidFor.getId();
            ensureUser(payerId);
            ensureUser(debtorId);

            balances.get(payerId).put(debtorId, balances.get(payerId).getOrDefault(debtorId, 0.0) + split.splitAmount);
            balances.get(debtorId).put(payerId, balances.get(debtorId).getOrDefault(payerId, 0.0) - split.splitAmount);
        }
    }

    public synchronized Map<String, Double> getBalance(String userId) {
        return new HashMap<>(balances.getOrDefault(userId, new HashMap<>()));
    }
}

class SplitWiseService {
    private final Map<String, User> usersRepository = new HashMap<>();
    private final Map<String, Group> groupRepository = new HashMap<>();
    private final BalanceManagerService balanceManagerService = new BalanceManagerService();

    public User createUser(String userId, String name) {
        User user = new User(userId, name);
        usersRepository.put(userId, user);
        return user;
    }

    public Group createGroup(String groupId, String name, List<User> users) {
        Group group = new Group(groupId, name);
        group.users = users;
        groupRepository.put(groupId, group);
        return group;
    }

    public Expense addExpense(String expenseId, User paidBy, double amount, List<User> usersParticipated, SplitStrategy strategy) {
        Expense expense = new Expense(expenseId, paidBy, amount, usersParticipated, strategy);
        List<Split> splits = expense.calculateShare();
        balanceManagerService.updateBalanceSheet(expense, splits);
        return expense;
    }

    public void showBalance(User user) {
        Map<String, Double> balances = balanceManagerService.getBalance(user.getId());
        boolean anyNonZero = false;
        for (Map.Entry<String, Double> entry : balances.entrySet()) {
            double amount = entry.getValue();
            if (amount == 0) {
                continue;
            }
            anyNonZero = true;
            if (amount > 0) {
                System.out.printf("%s owes %s %.2f%n", entry.getKey(), user.getName(), amount);
            } else {
                System.out.printf("%s owes %s %.2f%n", user.getName(), entry.getKey(), -amount);
            }
        }
        if (!anyNonZero) {
            System.out.println("No pending balances for " + user.getName());
        }
    }
}

public class Splitwise_LLD {
    public static void main(String[] args) {
        SplitWiseService service = new SplitWiseService();

        User alice = service.createUser("u1", "Alice");
        User bob = service.createUser("u2", "Bob");
        User carol = service.createUser("u3", "Carol");

        service.createGroup("g1", "Goa Trip", List.of(alice, bob, carol));

        System.out.println("Equal split: Alice pays 300 for all three");
        service.addExpense("e1", alice, 300.0, List.of(alice, bob, carol), new EqualSplit());

        System.out.println("Percentage split: Bob pays 200, Alice 30%, Carol 70%");
        Map<String, Double> percentages = new HashMap<>();
        percentages.put("u1", 30.0);
        percentages.put("u3", 70.0);
        service.addExpense("e2", bob, 200.0, List.of(alice, bob, carol), new PercentageSplit(percentages));

        for (User user : List.of(alice, bob, carol)) {
            System.out.println("\nBalances for " + user.getName() + ":");
            service.showBalance(user);
        }
    }
}
