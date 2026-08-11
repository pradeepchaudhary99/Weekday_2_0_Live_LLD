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

package main

import (
	"fmt"
	"sync"
)

type User struct {
	ID   string
	Name string
}

type Split struct {
	PaidBy      *User
	PaidFor     *User
	SplitAmount float64
	ExpenseID   string
}

type SplitStrategy interface {
	GenerateSplit(expense *Expense) []Split
}

type Expense struct {
	ID                string
	PaidBy            *User
	Amount            float64
	UsersParticipated []*User
	Strategy          SplitStrategy
	Splits            []Split
}

func (e *Expense) CalculateShare() []Split {
	e.Splits = e.Strategy.GenerateSplit(e)
	return e.Splits
}

// EqualSplit splits the amount equally among every participant except the payer.
type EqualSplit struct{}

func (EqualSplit) GenerateSplit(expense *Expense) []Split {
	var splits []Split
	if len(expense.UsersParticipated) == 0 {
		return splits
	}
	share := expense.Amount / float64(len(expense.UsersParticipated))
	for _, user := range expense.UsersParticipated {
		if user.ID == expense.PaidBy.ID {
			continue
		}
		splits = append(splits, Split{expense.PaidBy, user, share, expense.ID})
	}
	return splits
}

// PercentageSplit splits the amount according to a percentage assigned to each participant.
type PercentageSplit struct {
	Percentages map[string]float64
}

func (p PercentageSplit) GenerateSplit(expense *Expense) []Split {
	var splits []Split
	for _, user := range expense.UsersParticipated {
		if user.ID == expense.PaidBy.ID {
			continue
		}
		percentage := p.Percentages[user.ID]
		splits = append(splits, Split{expense.PaidBy, user, expense.Amount * percentage / 100.0, expense.ID})
	}
	return splits
}

type Group struct {
	ID       string
	Name     string
	Users    []*User
	Expenses []*Expense
}

// BalanceManagerService stores balances[a][b] = what user b owes user a.
type BalanceManagerService struct {
	mu       sync.Mutex
	balances map[string]map[string]float64
}

func NewBalanceManagerService() *BalanceManagerService {
	return &BalanceManagerService{balances: make(map[string]map[string]float64)}
}

func (b *BalanceManagerService) ensureUser(userID string) {
	if _, ok := b.balances[userID]; !ok {
		b.balances[userID] = make(map[string]float64)
	}
}

func (b *BalanceManagerService) UpdateBalanceSheet(_ *Expense, splits []Split) {
	b.mu.Lock()
	defer b.mu.Unlock()
	for _, split := range splits {
		payerID := split.PaidBy.ID
		debtorID := split.PaidFor.ID
		b.ensureUser(payerID)
		b.ensureUser(debtorID)

		b.balances[payerID][debtorID] += split.SplitAmount
		b.balances[debtorID][payerID] -= split.SplitAmount
	}
}

func (b *BalanceManagerService) GetBalance(userID string) map[string]float64 {
	b.mu.Lock()
	defer b.mu.Unlock()
	result := make(map[string]float64)
	for k, v := range b.balances[userID] {
		result[k] = v
	}
	return result
}

type SplitWiseService struct {
	usersRepository       map[string]*User
	groupRepository       map[string]*Group
	balanceManagerService *BalanceManagerService
}

func NewSplitWiseService() *SplitWiseService {
	return &SplitWiseService{
		usersRepository:       make(map[string]*User),
		groupRepository:       make(map[string]*Group),
		balanceManagerService: NewBalanceManagerService(),
	}
}

func (s *SplitWiseService) CreateUser(userID, name string) *User {
	user := &User{ID: userID, Name: name}
	s.usersRepository[userID] = user
	return user
}

func (s *SplitWiseService) CreateGroup(groupID, name string, users []*User) *Group {
	group := &Group{ID: groupID, Name: name, Users: users}
	s.groupRepository[groupID] = group
	return group
}

func (s *SplitWiseService) AddExpense(expenseID string, paidBy *User, amount float64,
	usersParticipated []*User, strategy SplitStrategy) *Expense {
	expense := &Expense{ID: expenseID, PaidBy: paidBy, Amount: amount,
		UsersParticipated: usersParticipated, Strategy: strategy}
	splits := expense.CalculateShare()
	s.balanceManagerService.UpdateBalanceSheet(expense, splits)
	return expense
}

func (s *SplitWiseService) ShowBalance(user *User) {
	balances := s.balanceManagerService.GetBalance(user.ID)
	anyNonZero := false
	for otherID, amount := range balances {
		if amount == 0 {
			continue
		}
		anyNonZero = true
		if amount > 0 {
			fmt.Printf("%s owes %s %.2f\n", otherID, user.Name, amount)
		} else {
			fmt.Printf("%s owes %s %.2f\n", user.Name, otherID, -amount)
		}
	}
	if !anyNonZero {
		fmt.Printf("No pending balances for %s\n", user.Name)
	}
}

func main() {
	service := NewSplitWiseService()

	alice := service.CreateUser("u1", "Alice")
	bob := service.CreateUser("u2", "Bob")
	carol := service.CreateUser("u3", "Carol")

	service.CreateGroup("g1", "Goa Trip", []*User{alice, bob, carol})

	fmt.Println("Equal split: Alice pays 300 for all three")
	service.AddExpense("e1", alice, 300.0, []*User{alice, bob, carol}, EqualSplit{})

	fmt.Println("Percentage split: Bob pays 200, Alice 30%, Carol 70%")
	percentages := map[string]float64{"u1": 30.0, "u3": 70.0}
	service.AddExpense("e2", bob, 200.0, []*User{alice, bob, carol}, PercentageSplit{percentages})

	for _, user := range []*User{alice, bob, carol} {
		fmt.Printf("\nBalances for %s:\n", user.Name)
		service.ShowBalance(user)
	}
}
