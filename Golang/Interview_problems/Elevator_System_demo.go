package main

/*
Elevator System

User should be able to request the elevator
Elevator system should be managing any number of elevators
Our system should support both internal and external requests
Elevator system should support multiple type of elevatorAssignment algorithms
elevator system should support safety measures for the users

------ Non Functional --------
Extensibility
Thread-safety
No duplicate request
Fault isolaton
/// Performance...
*/

// Core Entities
// Elevator
// Request
// SchedulingStrategy
// NearestElevatorStrategy
// ElevatorSystem // ElevatorController
//

import "time"

type Direction int

const (
	Up Direction = iota
	Down
	Idle
)

type ElevatorState int

const (
	Moving ElevatorState = iota
	Stopped
	StateIdle
	Maintainence
)

type DoorState int

const (
	Open DoorState = iota
	Closed
)

type Request struct {
	Floor     int
	Direction Direction
}

type SchedulingStrategy interface {
	GetElevator(elevators []*Elevator, request *Request) *Elevator
}

type NearestElevatorStrategy struct{}

func (s *NearestElevatorStrategy) GetElevator(elevators []*Elevator, request *Request) *Elevator {
	return nil
}

// sortedFloors mirrors java.util.TreeSet ordering (ascending or descending).
type sortedFloors struct {
	items      []int
	descending bool
}

func newSortedFloors(descending bool) *sortedFloors {
	return &sortedFloors{descending: descending}
}

func (s *sortedFloors) Add(value int) {
	index := len(s.items)
	for i, item := range s.items {
		if (s.descending && item < value) || (!s.descending && item > value) {
			index = i
			break
		}
	}
	s.items = append(s.items, 0)
	copy(s.items[index+1:], s.items[index:])
	s.items[index] = value
}

func (s *sortedFloors) IsEmpty() bool {
	return len(s.items) == 0
}

func (s *sortedFloors) First() int {
	return s.items[0]
}

func (s *sortedFloors) PollFirst() int {
	head := s.items[0]
	s.items = s.items[1:]
	return head
}

type Elevator struct {
	ID           string
	State        ElevatorState
	DoorState    DoorState
	CurrentFloor int
	UpStops      *sortedFloors // Increasing Order
	DownStops    *sortedFloors // Decreasing Order
	Running      bool
	Direction    Direction
}

func NewElevator() *Elevator {
	return &Elevator{
		State:     StateIdle,
		DoorState: Closed,
		UpStops:   newSortedFloors(false),
		DownStops: newSortedFloors(true),
		Direction: Idle,
	}
}

func (e *Elevator) StartElevator() {
	e.Running = true
}

func (e *Elevator) StopElevator() {
	e.Running = false
}

func (e *Elevator) AddRequest(request *Request) {
	if request.Floor > e.CurrentFloor {
		e.UpStops.Add(request.Floor)
	} else if request.Floor < e.CurrentFloor {
		e.DownStops.Add(request.Floor)
	}
}

func (e *Elevator) Run() {
	for e.Running {
		e.Step()
		time.Sleep(150 * time.Millisecond) // simulate
	}
}

func (e *Elevator) Step() {
	dir := e.Direction
	if dir == Up {
		if e.UpStops.IsEmpty() {
			if e.DownStops.IsEmpty() {
				e.Direction = Idle
				e.State = StateIdle
			} else {
				e.Direction = Down
				e.State = Moving
			}
			return
		}
		if e.CurrentFloor == e.UpStops.First() {
			e.UpStops.PollFirst()
			// STOPPING the Elevator
			// 3-4 lines of logic for opening/closing the doors
		}
		e.CurrentFloor++
	} else if dir == Down {
		if e.DownStops.IsEmpty() {
			if e.UpStops.IsEmpty() {
				e.Direction = Idle
				e.State = StateIdle
			} else {
				e.Direction = Up
				e.State = Moving
			}
			return
		}
		if e.CurrentFloor == e.DownStops.First() {
			e.DownStops.PollFirst()
			// STOPPING the Elevator
			// 3-4 lines of logic for opening/closing the doors
		}
		e.CurrentFloor--
	} else {
		e.State = StateIdle
	}
}

type ElevatorSystem struct {
	Elevators         []*Elevator
	Strategy          SchedulingStrategy
	NumberOfElevators int
}

func NewElevatorSystem() *ElevatorSystem {
	system := &ElevatorSystem{NumberOfElevators: 5}
	for i := 1; i <= 5; i++ {
		system.Elevators = append(system.Elevators, NewElevator())
	}
	return system
}

func (s *ElevatorSystem) AddRequest(request *Request) {
	elevator := s.Strategy.GetElevator(s.Elevators, request)
	elevator.AddRequest(request)
}
