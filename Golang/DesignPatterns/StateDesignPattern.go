package main

import "fmt"

type IATMMachineState interface {
	InsertCard()
	WithdrawCash()
	RemoveCard()
	PressCancel()
}

// ATM: CardInsertedState, NoMoneyState, HasMoneyState, NoCardState

type CardInsertedState struct {
	atm *ATMMachine
}

func (s *CardInsertedState) InsertCard() {
	fmt.Println("Already Inserted No use")
}

func (s *CardInsertedState) WithdrawCash() {
	fmt.Println("Some 100 Line  of code you are running")
}

func (s *CardInsertedState) RemoveCard() {
	fmt.Println("Card is removed")
	s.atm.SetState(s.atm.GetNoCardState()) // logic for transitioning the state
}

func (s *CardInsertedState) PressCancel() {
	s.RemoveCard()
}

type NoCardInsertedState struct {
	atm *ATMMachine
}

func (s *NoCardInsertedState) InsertCard()   {}
func (s *NoCardInsertedState) WithdrawCash() {}
func (s *NoCardInsertedState) RemoveCard()   {}
func (s *NoCardInsertedState) PressCancel()  {}

type HasMoneyState struct {
	atm *ATMMachine
}

func (s *HasMoneyState) InsertCard()   {}
func (s *HasMoneyState) WithdrawCash() {}
func (s *HasMoneyState) RemoveCard()   {}
func (s *HasMoneyState) PressCancel()  {}

type ATMMachine struct {
	currentState        IATMMachineState
	cardInsertedState   IATMMachineState
	noCardInsertedState IATMMachineState
	hasMoneyState       IATMMachineState
}

func NewATMMachine() *ATMMachine {
	atm := &ATMMachine{}
	atm.noCardInsertedState = &NoCardInsertedState{atm: atm}
	atm.cardInsertedState = &CardInsertedState{atm: atm}
	atm.hasMoneyState = &HasMoneyState{atm: atm}
	atm.currentState = atm.noCardInsertedState
	return atm
}

func (a *ATMMachine) SetState(state IATMMachineState) {
	a.currentState = state
}

func (a *ATMMachine) GetNoCardState() IATMMachineState {
	return a.noCardInsertedState
}

func (a *ATMMachine) GetCardInsertedState() IATMMachineState {
	return a.noCardInsertedState
}

func (a *ATMMachine) GetHasMoneyState() IATMMachineState {
	return a.noCardInsertedState
}

func (a *ATMMachine) InsertCard()   { a.currentState.InsertCard() }
func (a *ATMMachine) WithdrawCash() { a.currentState.WithdrawCash() }
func (a *ATMMachine) RemoveCard()   { a.currentState.RemoveCard() }
func (a *ATMMachine) PressCancel()  { a.currentState.PressCancel() }

func main() {}
