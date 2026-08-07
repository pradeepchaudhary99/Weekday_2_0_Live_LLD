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
       mean adding a new type, not editing existing ones.

Design (Strategy pattern):
    SplitStrategy is the interface every split type implements. It turns
    "$total among these participants" into a slice of per-user Splits.
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

package main

import (
	"fmt"
	"math"
	"sort"
)

type User struct {
	ID   string
	Name string
}

type Split struct {
	User   User
	Amount float64
}

type SplitStrategy interface {
	Calculate(totalAmount float64, participants []User, values map[string]float64) ([]Split, error)
}

type EqualSplitStrategy struct{}

func (EqualSplitStrategy) Calculate(totalAmount float64, participants []User, values map[string]float64) ([]Split, error) {
	splits := make([]Split, 0, len(participants))
	n := int64(len(participants))
	// Round each share to cents, then hand the leftover cents (caused by
	// rounding) to the first participants so the splits sum exactly to
	// totalAmount instead of drifting by a cent or two.
	totalCents := int64(math.Round(totalAmount * 100))
	baseShareCents := totalCents / n
	remainderCents := totalCents % n
	for i, user := range participants {
		shareCents := baseShareCents
		if int64(i) < remainderCents {
			shareCents++
		}
		splits = append(splits, Split{User: user, Amount: float64(shareCents) / 100.0})
	}
	return splits, nil
}

type ExactSplitStrategy struct{}

func (ExactSplitStrategy) Calculate(totalAmount float64, participants []User, values map[string]float64) ([]Split, error) {
	splits := make([]Split, 0, len(participants))
	sum := 0.0
	for _, user := range participants {
		amount, ok := values[user.ID]
		if !ok {
			return nil, fmt.Errorf("missing exact amount for user %s", user.ID)
		}
		sum += amount
		splits = append(splits, Split{User: user, Amount: amount})
	}
	if math.Abs(sum-totalAmount) > 0.01 {
		return nil, fmt.Errorf("exact amounts (%.2f) do not add up to the total (%.2f)", sum, totalAmount)
	}
	return splits, nil
}

type PercentSplitStrategy struct{}

func (PercentSplitStrategy) Calculate(totalAmount float64, participants []User, values map[string]float64) ([]Split, error) {
	splits := make([]Split, 0, len(participants))
	percentSum := 0.0
	for _, user := range participants {
		percent, ok := values[user.ID]
		if !ok {
			return nil, fmt.Errorf("missing percentage for user %s", user.ID)
		}
		percentSum += percent
		splits = append(splits, Split{User: user, Amount: totalAmount * percent / 100.0})
	}
	if math.Abs(percentSum-100.0) > 0.01 {
		return nil, fmt.Errorf("percentages (%.2f) do not add up to 100", percentSum)
	}
	return splits, nil
}

type SplitType int

const (
	Equal SplitType = iota
	Exact
	Percent
)

type Expense struct {
	Description string
	Amount      float64
	PaidBy      User
	Splits      []Split
}

// BalanceSheet tracks the net, directional debt between every pair of
// users: debts[X][Y] = amount X still owes Y. Only one of debts[X][Y] /
// debts[Y][X] is ever non-zero at a time.
type BalanceSheet struct {
	debts map[string]map[string]float64
}

func NewBalanceSheet() *BalanceSheet {
	return &BalanceSheet{debts: make(map[string]map[string]float64)}
}

func (b *BalanceSheet) AddDebt(debtorID, creditorID string, amount float64) {
	if debtorID == creditorID || amount <= 0 {
		return
	}
	owedBack := b.getDebt(creditorID, debtorID)
	if owedBack >= amount {
		b.setDebt(creditorID, debtorID, owedBack-amount)
	} else {
		b.setDebt(creditorID, debtorID, 0)
		b.setDebt(debtorID, creditorID, b.getDebt(debtorID, creditorID)+(amount-owedBack))
	}
}

func (b *BalanceSheet) getDebt(debtorID, creditorID string) float64 {
	if inner, ok := b.debts[debtorID]; ok {
		return inner[creditorID]
	}
	return 0
}

func (b *BalanceSheet) setDebt(debtorID, creditorID string, amount float64) {
	if _, ok := b.debts[debtorID]; !ok {
		b.debts[debtorID] = make(map[string]float64)
	}
	b.debts[debtorID][creditorID] = amount
}

func (b *BalanceSheet) PrintBalances() {
	debtorIDs := make([]string, 0, len(b.debts))
	for debtorID := range b.debts {
		debtorIDs = append(debtorIDs, debtorID)
	}
	sort.Strings(debtorIDs)

	any := false
	for _, debtorID := range debtorIDs {
		creditorIDs := make([]string, 0, len(b.debts[debtorID]))
		for creditorID := range b.debts[debtorID] {
			creditorIDs = append(creditorIDs, creditorID)
		}
		sort.Strings(creditorIDs)
		for _, creditorID := range creditorIDs {
			amount := b.debts[debtorID][creditorID]
			if amount > 0.005 {
				fmt.Printf("  %s owes %s: %.2f\n", debtorID, creditorID, amount)
				any = true
			}
		}
	}
	if !any {
		fmt.Println("  Everyone is settled up.")
	}
}

type SplitwiseService struct {
	users        map[string]User
	balanceSheet *BalanceSheet
	strategies   map[SplitType]SplitStrategy
}

func NewSplitwiseService() *SplitwiseService {
	return &SplitwiseService{
		users:        make(map[string]User),
		balanceSheet: NewBalanceSheet(),
		strategies: map[SplitType]SplitStrategy{
			Equal:   EqualSplitStrategy{},
			Exact:   ExactSplitStrategy{},
			Percent: PercentSplitStrategy{},
		},
	}
}

func (s *SplitwiseService) AddUser(user User) {
	s.users[user.ID] = user
}

func (s *SplitwiseService) AddExpense(description string, amount float64, paidByUserID string,
	participantIDs []string, splitType SplitType, values map[string]float64) (Expense, error) {
	paidBy := s.users[paidByUserID]
	participants := make([]User, 0, len(participantIDs))
	for _, id := range participantIDs {
		participants = append(participants, s.users[id])
	}

	splits, err := s.strategies[splitType].Calculate(amount, participants, values)
	if err != nil {
		return Expense{}, err
	}

	for _, split := range splits {
		if split.User.ID != paidByUserID {
			s.balanceSheet.AddDebt(split.User.ID, paidByUserID, split.Amount)
		}
	}

	return Expense{Description: description, Amount: amount, PaidBy: paidBy, Splits: splits}, nil
}

func (s *SplitwiseService) PrintBalances() {
	s.balanceSheet.PrintBalances()
}

func main() {
	service := NewSplitwiseService()
	service.AddUser(User{ID: "alice", Name: "Alice"})
	service.AddUser(User{ID: "bob", Name: "Bob"})
	service.AddUser(User{ID: "charlie", Name: "Charlie"})
	service.AddUser(User{ID: "dave", Name: "Dave"})

	fmt.Println("Alice pays 400.00, split EQUALLY among alice, bob, charlie, dave")
	service.AddExpense("Dinner", 400.00, "alice", []string{"alice", "bob", "charlie", "dave"}, Equal, nil)
	service.PrintBalances()

	fmt.Println("\nBob pays 300.00, split EXACTLY: alice=100, bob=50, charlie=150")
	service.AddExpense("Groceries", 300.00, "bob", []string{"alice", "bob", "charlie"}, Exact,
		map[string]float64{"alice": 100.0, "bob": 50.0, "charlie": 150.0})
	service.PrintBalances()

	fmt.Println("\nCharlie pays 1000.00, split by PERCENT: alice=20%, bob=30%, charlie=50%")
	service.AddExpense("Trip", 1000.00, "charlie", []string{"alice", "bob", "charlie"}, Percent,
		map[string]float64{"alice": 20.0, "bob": 30.0, "charlie": 50.0})
	service.PrintBalances()

	fmt.Println("\nAttempting an EXACT split that doesn't add up to the total:")
	_, err := service.AddExpense("Bad split", 100.00, "dave", []string{"alice", "dave"}, Exact,
		map[string]float64{"alice": 40.0, "dave": 50.0})
	if err != nil {
		fmt.Printf("  Rejected: %v\n", err)
	}
}
