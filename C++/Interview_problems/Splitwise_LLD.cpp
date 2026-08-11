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

#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

struct User {
    std::string id;
    std::string name;

    User(std::string id, std::string name) : id(std::move(id)), name(std::move(name)) {}
};

class Expense;

struct Split {
    std::shared_ptr<User> paidBy;
    std::shared_ptr<User> paidFor;
    double splitAmount;
    std::string expenseId;

    Split(std::shared_ptr<User> paidBy, std::shared_ptr<User> paidFor, double splitAmount, std::string expenseId)
        : paidBy(std::move(paidBy)), paidFor(std::move(paidFor)), splitAmount(splitAmount),
          expenseId(std::move(expenseId)) {}
};

struct SplitStrategy {
    virtual ~SplitStrategy() = default;
    virtual std::vector<Split> generateSplit(const Expense& expense) const = 0;
};

class Expense {
public:
    std::string id;
    std::shared_ptr<User> paidBy;
    double amount;
    std::vector<std::shared_ptr<User>> usersParticipated;
    std::shared_ptr<SplitStrategy> strategy;
    std::vector<Split> splits;

    Expense(std::string id, std::shared_ptr<User> paidBy, double amount,
            std::vector<std::shared_ptr<User>> usersParticipated, std::shared_ptr<SplitStrategy> strategy)
        : id(std::move(id)), paidBy(std::move(paidBy)), amount(amount),
          usersParticipated(std::move(usersParticipated)), strategy(std::move(strategy)) {}

    std::vector<Split> calculateShare() {
        splits = strategy->generateSplit(*this);
        return splits;
    }
};

// Splits the amount equally among every participant except the payer.
class EqualSplit : public SplitStrategy {
public:
    std::vector<Split> generateSplit(const Expense& expense) const override {
        std::vector<Split> splits;
        if (expense.usersParticipated.empty()) {
            return splits;
        }
        double share = expense.amount / static_cast<double>(expense.usersParticipated.size());
        for (const auto& user : expense.usersParticipated) {
            if (user->id == expense.paidBy->id) {
                continue;
            }
            splits.emplace_back(expense.paidBy, user, share, expense.id);
        }
        return splits;
    }
};

// Splits the amount according to a percentage assigned to each participant.
class PercentageSplit : public SplitStrategy {
public:
    explicit PercentageSplit(std::map<std::string, double> percentages) : percentages_(std::move(percentages)) {}

    std::vector<Split> generateSplit(const Expense& expense) const override {
        std::vector<Split> splits;
        for (const auto& user : expense.usersParticipated) {
            if (user->id == expense.paidBy->id) {
                continue;
            }
            auto it = percentages_.find(user->id);
            double percentage = it != percentages_.end() ? it->second : 0.0;
            splits.emplace_back(expense.paidBy, user, expense.amount * percentage / 100.0, expense.id);
        }
        return splits;
    }

private:
    std::map<std::string, double> percentages_;
};

struct Group {
    std::string id;
    std::string name;
    std::vector<std::shared_ptr<User>> users;
    std::vector<std::shared_ptr<Expense>> expenses;

    Group(std::string id, std::string name) : id(std::move(id)), name(std::move(name)) {}
};

// balances[a][b] is what user b owes user a.
class BalanceManagerService {
public:
    void updateBalanceSheet(const Expense& /*expense*/, const std::vector<Split>& splits) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& split : splits) {
            const std::string& payerId = split.paidBy->id;
            const std::string& debtorId = split.paidFor->id;
            ensureUser(payerId);
            ensureUser(debtorId);

            balances_[payerId][debtorId] += split.splitAmount;
            balances_[debtorId][payerId] -= split.splitAmount;
        }
    }

    std::map<std::string, double> getBalance(const std::string& userId) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = balances_.find(userId);
        if (it == balances_.end()) {
            return {};
        }
        return it->second;
    }

private:
    void ensureUser(const std::string& userId) {
        balances_.emplace(userId, std::map<std::string, double>{});
    }

    std::mutex mutex_;
    std::map<std::string, std::map<std::string, double>> balances_;
};

class SplitWiseService {
public:
    std::shared_ptr<User> createUser(const std::string& userId, const std::string& name) {
        auto user = std::make_shared<User>(userId, name);
        usersRepository_[userId] = user;
        return user;
    }

    std::shared_ptr<Group> createGroup(const std::string& groupId, const std::string& name,
                                        const std::vector<std::shared_ptr<User>>& users) {
        auto group = std::make_shared<Group>(groupId, name);
        group->users = users;
        groupRepository_[groupId] = group;
        return group;
    }

    std::shared_ptr<Expense> addExpense(const std::string& expenseId, const std::shared_ptr<User>& paidBy,
                                         double amount, const std::vector<std::shared_ptr<User>>& usersParticipated,
                                         const std::shared_ptr<SplitStrategy>& strategy) {
        auto expense = std::make_shared<Expense>(expenseId, paidBy, amount, usersParticipated, strategy);
        std::vector<Split> splits = expense->calculateShare();
        balanceManagerService_.updateBalanceSheet(*expense, splits);
        return expense;
    }

    void showBalance(const User& user) {
        std::map<std::string, double> balances = balanceManagerService_.getBalance(user.id);
        bool anyNonZero = false;
        for (const auto& [otherId, amount] : balances) {
            if (amount == 0) {
                continue;
            }
            anyNonZero = true;
            if (amount > 0) {
                std::printf("%s owes %s %.2f\n", otherId.c_str(), user.name.c_str(), amount);
            } else {
                std::printf("%s owes %s %.2f\n", user.name.c_str(), otherId.c_str(), -amount);
            }
        }
        if (!anyNonZero) {
            std::cout << "No pending balances for " << user.name << "\n";
        }
    }

private:
    std::map<std::string, std::shared_ptr<User>> usersRepository_;
    std::map<std::string, std::shared_ptr<Group>> groupRepository_;
    BalanceManagerService balanceManagerService_;
};

int main() {
    SplitWiseService service;

    auto alice = service.createUser("u1", "Alice");
    auto bob = service.createUser("u2", "Bob");
    auto carol = service.createUser("u3", "Carol");

    service.createGroup("g1", "Goa Trip", {alice, bob, carol});

    std::cout << "Equal split: Alice pays 300 for all three\n";
    service.addExpense("e1", alice, 300.0, {alice, bob, carol}, std::make_shared<EqualSplit>());

    std::cout << "Percentage split: Bob pays 200, Alice 30%, Carol 70%\n";
    std::map<std::string, double> percentages = {{"u1", 30.0}, {"u3", 70.0}};
    service.addExpense("e2", bob, 200.0, {alice, bob, carol}, std::make_shared<PercentageSplit>(percentages));

    for (const auto& user : {alice, bob, carol}) {
        std::cout << "\nBalances for " << user->name << ":\n";
        service.showBalance(*user);
    }

    return 0;
}
