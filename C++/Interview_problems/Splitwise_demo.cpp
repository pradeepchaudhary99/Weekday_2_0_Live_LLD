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

#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct User {
    std::string id;
    std::string name;
};

struct Split {
    User user;
    double amount;
};

struct SplitStrategy {
    virtual std::vector<Split> calculate(double totalAmount, const std::vector<User>& participants,
                                          const std::map<std::string, double>& values) = 0;
    virtual ~SplitStrategy() = default;
};

class EqualSplitStrategy : public SplitStrategy {
public:
    std::vector<Split> calculate(double totalAmount, const std::vector<User>& participants,
                                  const std::map<std::string, double>& values) override {
        std::vector<Split> splits;
        int n = static_cast<int>(participants.size());
        // Round each share to cents, then hand the leftover cents (caused by
        // rounding) to the first participants so the splits sum exactly to
        // totalAmount instead of drifting by a cent or two.
        long long totalCents = std::llround(totalAmount * 100);
        long long baseShareCents = totalCents / n;
        long long remainderCents = totalCents % n;
        for (int i = 0; i < n; i++) {
            long long shareCents = baseShareCents + (i < remainderCents ? 1 : 0);
            splits.push_back({participants[i], shareCents / 100.0});
        }
        return splits;
    }
};

class ExactSplitStrategy : public SplitStrategy {
public:
    std::vector<Split> calculate(double totalAmount, const std::vector<User>& participants,
                                  const std::map<std::string, double>& values) override {
        std::vector<Split> splits;
        double sum = 0;
        for (const auto& user : participants) {
            auto it = values.find(user.id);
            if (it == values.end()) {
                throw std::invalid_argument("Missing exact amount for user " + user.id);
            }
            sum += it->second;
            splits.push_back({user, it->second});
        }
        if (std::abs(sum - totalAmount) > 0.01) {
            throw std::invalid_argument("Exact amounts (" + std::to_string(sum) +
                                         ") do not add up to the total (" + std::to_string(totalAmount) + ")");
        }
        return splits;
    }
};

class PercentSplitStrategy : public SplitStrategy {
public:
    std::vector<Split> calculate(double totalAmount, const std::vector<User>& participants,
                                  const std::map<std::string, double>& values) override {
        std::vector<Split> splits;
        double percentSum = 0;
        for (const auto& user : participants) {
            auto it = values.find(user.id);
            if (it == values.end()) {
                throw std::invalid_argument("Missing percentage for user " + user.id);
            }
            percentSum += it->second;
            splits.push_back({user, totalAmount * it->second / 100.0});
        }
        if (std::abs(percentSum - 100.0) > 0.01) {
            throw std::invalid_argument("Percentages (" + std::to_string(percentSum) + ") do not add up to 100");
        }
        return splits;
    }
};

enum class SplitType { EQUAL, EXACT, PERCENT };

struct Expense {
    std::string description;
    double amount;
    User paidBy;
    std::vector<Split> splits;
};

// Tracks the net, directional debt between every pair of users:
// debts[X][Y] = amount X still owes Y. Only one of debts[X][Y] / debts[Y][X]
// is ever non-zero at a time.
class BalanceSheet {
public:
    void addDebt(const std::string& debtorId, const std::string& creditorId, double amount) {
        if (debtorId == creditorId || amount <= 0) {
            return;
        }
        double owedBack = getDebt(creditorId, debtorId);
        if (owedBack >= amount) {
            setDebt(creditorId, debtorId, owedBack - amount);
        } else {
            setDebt(creditorId, debtorId, 0.0);
            setDebt(debtorId, creditorId, getDebt(debtorId, creditorId) + (amount - owedBack));
        }
    }

    void printBalances() const {
        bool any = false;
        for (const auto& [debtorId, owedMap] : debts_) {
            for (const auto& [creditorId, amount] : owedMap) {
                if (amount > 0.005) {
                    printf("  %s owes %s: %.2f\n", debtorId.c_str(), creditorId.c_str(), amount);
                    any = true;
                }
            }
        }
        if (!any) {
            std::cout << "  Everyone is settled up." << std::endl;
        }
    }

private:
    double getDebt(const std::string& debtorId, const std::string& creditorId) const {
        auto it = debts_.find(debtorId);
        if (it == debts_.end()) return 0.0;
        auto it2 = it->second.find(creditorId);
        if (it2 == it->second.end()) return 0.0;
        return it2->second;
    }

    void setDebt(const std::string& debtorId, const std::string& creditorId, double amount) {
        debts_[debtorId][creditorId] = amount;
    }

    std::map<std::string, std::map<std::string, double>> debts_;
};

class SplitwiseService {
public:
    SplitwiseService() {
        strategies_[SplitType::EQUAL] = std::make_unique<EqualSplitStrategy>();
        strategies_[SplitType::EXACT] = std::make_unique<ExactSplitStrategy>();
        strategies_[SplitType::PERCENT] = std::make_unique<PercentSplitStrategy>();
    }

    void addUser(const User& user) {
        users_[user.id] = user;
    }

    Expense addExpense(const std::string& description, double amount, const std::string& paidByUserId,
                        const std::vector<std::string>& participantIds, SplitType type,
                        const std::map<std::string, double>& values = {}) {
        User paidBy = users_.at(paidByUserId);
        std::vector<User> participants;
        for (const auto& id : participantIds) {
            participants.push_back(users_.at(id));
        }

        std::vector<Split> splits = strategies_[type]->calculate(amount, participants, values);

        for (const auto& split : splits) {
            if (split.user.id != paidByUserId) {
                balanceSheet_.addDebt(split.user.id, paidByUserId, split.amount);
            }
        }

        return {description, amount, paidBy, splits};
    }

    void printBalances() const {
        balanceSheet_.printBalances();
    }

private:
    std::map<std::string, User> users_;
    BalanceSheet balanceSheet_;
    std::map<SplitType, std::unique_ptr<SplitStrategy>> strategies_;
};

int main() {
    SplitwiseService service;
    service.addUser({"alice", "Alice"});
    service.addUser({"bob", "Bob"});
    service.addUser({"charlie", "Charlie"});
    service.addUser({"dave", "Dave"});

    std::cout << "Alice pays 400.00, split EQUALLY among alice, bob, charlie, dave" << std::endl;
    service.addExpense("Dinner", 400.00, "alice", {"alice", "bob", "charlie", "dave"}, SplitType::EQUAL);
    service.printBalances();

    std::cout << "\nBob pays 300.00, split EXACTLY: alice=100, bob=50, charlie=150" << std::endl;
    service.addExpense("Groceries", 300.00, "bob", {"alice", "bob", "charlie"}, SplitType::EXACT,
                        {{"alice", 100.0}, {"bob", 50.0}, {"charlie", 150.0}});
    service.printBalances();

    std::cout << "\nCharlie pays 1000.00, split by PERCENT: alice=20%, bob=30%, charlie=50%" << std::endl;
    service.addExpense("Trip", 1000.00, "charlie", {"alice", "bob", "charlie"}, SplitType::PERCENT,
                        {{"alice", 20.0}, {"bob", 30.0}, {"charlie", 50.0}});
    service.printBalances();

    std::cout << "\nAttempting an EXACT split that doesn't add up to the total:" << std::endl;
    try {
        service.addExpense("Bad split", 100.00, "dave", {"alice", "dave"}, SplitType::EXACT,
                            {{"alice", 40.0}, {"dave", 50.0}});
    } catch (const std::invalid_argument& e) {
        std::cout << "  Rejected: " << e.what() << std::endl;
    }

    return 0;
}
