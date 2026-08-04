/*
================================================================================
LLD: Elevator System
================================================================================

Functional Requirements:
    1. A user can request an elevator (an external hall call: "I'm on floor
       5 and want to go up") and can request a destination from inside a
       cabin (an internal call: "take me to floor 9").
    2. The system manages any number of elevators as one fleet.
    3. Both internal and external requests are supported through the same
       Request abstraction.
    4. The system supports multiple elevator-assignment algorithms (which
       elevator should service a given request) behind one interface.
    5. Elevators expose door state and movement state so unsafe actions
       (e.g. moving with doors open) are structurally impossible to model.

Non-Functional Requirements:
    1. Extensibility: a new assignment algorithm is a new SchedulingStrategy
       implementation, not a rewrite of ElevatorSystem.
    2. Thread-safety: each elevator runs on its own goroutine and mutates
       its own stop-queues/state; those fields are guarded by a per-elevator
       mutex since requests can arrive concurrently from many callers.
    3. No duplicate requests: stop floors are kept in a sorted set, so
       requesting the same floor twice is a no-op, not a double visit.
    4. Fault isolation: one elevator's goroutine never touches another
       elevator's state, so a bug/slowdown in one cabin can't corrupt or
       block the rest of the fleet.

--------------------------------------------------------------------------
Design
--------------------------------------------------------------------------
Strategy pattern for assignment:
    ElevatorSystem never hardcodes "closest elevator wins" -- it asks a
    SchedulingStrategy. NearestElevatorStrategy is one implementation;
    swapping in e.g. a "least busy" or SCAN-based strategy means adding a
    type, not editing ElevatorSystem.

Sorted stop-queues drive the movement loop:
    Each Elevator keeps two sorted-floor sets: upStops (visited ascending,
    so the smallest is always the next stop while moving up) and
    downStops (visited descending, so the largest is always the next stop
    while moving down). One Step() call either advances one floor or
    services a stop; the background goroutine just calls Step() on a fixed
    cadence -- this is what turns "the elevator has some destinations"
    into actual motion without busy-polling a shared queue.

Core Entities:
    Direction             -- Up, Down, Idle
    ElevatorState          -- Moving, Stopped, StateIdle, Maintenance
    DoorState              -- Open, Closed
    Request                -- Floor (+ Direction, for external hall calls)
    SchedulingStrategy      -- assignment interface
    NearestElevatorStrategy
    Elevator               -- one cabin's state machine + movement goroutine
    ElevatorSystem          -- owns the fleet, routes requests via the strategy
================================================================================
*/

package main

import (
	"fmt"
	"sync"
	"time"
)

type Direction int

const (
	Up Direction = iota
	Down
	DirectionIdle
)

type ElevatorState int

const (
	Moving ElevatorState = iota
	Stopped
	StateIdle
	Maintenance
)

type DoorState int

const (
	Open DoorState = iota
	Closed
)

type Request struct {
	Floor     int
	Direction Direction // only meaningful for an external hall call
}

type SchedulingStrategy interface {
	GetElevator(elevators []*Elevator, request *Request) *Elevator
}

// NearestElevatorStrategy picks whichever elevator is numerically closest
// to the requested floor, with a preference for elevators that are
// currently idle (an idle elevator can retarget immediately; a moving one
// should ideally finish its current direction first, which a fancier
// SCAN-style strategy would model -- left as an extension point).
type NearestElevatorStrategy struct{}

func (s *NearestElevatorStrategy) GetElevator(elevators []*Elevator, request *Request) *Elevator {
	var best *Elevator
	bestScore := int(^uint(0) >> 1) // max int
	for _, elevator := range elevators {
		floor := elevator.CurrentFloor()
		idle := elevator.IsIdle()
		distance := abs(floor - request.Floor)
		// Idle elevators are scored as if they were half as far away, so a
		// busy elevator only wins when it is clearly closer.
		score := distance
		if !idle {
			score = distance * 2
		}
		if score < bestScore {
			bestScore = score
			best = elevator
		}
	}
	return best
}

func abs(n int) int {
	if n < 0 {
		return -n
	}
	return n
}

// sortedFloors is a minimal ascending sorted-set of floor numbers: O(n)
// insert/remove, which is fine at the "handful of pending stops" scale a
// single elevator ever queues up.
type sortedFloors struct {
	items []int
}

func (s *sortedFloors) Add(value int) {
	index := len(s.items)
	for i, item := range s.items {
		if item == value {
			return // no duplicate stops
		}
		if item > value {
			index = i
			break
		}
	}
	s.items = append(s.items, 0)
	copy(s.items[index+1:], s.items[index:])
	s.items[index] = value
}

func (s *sortedFloors) IsEmpty() bool { return len(s.items) == 0 }

func (s *sortedFloors) First() int { return s.items[0] }

func (s *sortedFloors) Last() int { return s.items[len(s.items)-1] }

func (s *sortedFloors) PollFirst() int {
	head := s.items[0]
	s.items = s.items[1:]
	return head
}

func (s *sortedFloors) PollLast() int {
	last := s.items[len(s.items)-1]
	s.items = s.items[:len(s.items)-1]
	return last
}

type Elevator struct {
	ID int

	mutex        sync.Mutex
	state        ElevatorState
	doorState    DoorState
	currentFloor int
	direction    Direction
	upStops      sortedFloors // visited ascending
	downStops    sortedFloors // visited descending (via Last/PollLast)
	running      bool
}

func NewElevator(id int, startingFloor int) *Elevator {
	return &Elevator{
		ID:           id,
		state:        StateIdle,
		doorState:    Closed,
		currentFloor: startingFloor,
		direction:    DirectionIdle,
	}
}

func (e *Elevator) StartElevator() {
	e.mutex.Lock()
	defer e.mutex.Unlock()
	e.running = true
}

func (e *Elevator) StopElevator() {
	e.mutex.Lock()
	defer e.mutex.Unlock()
	e.running = false
}

func (e *Elevator) isRunning() bool {
	e.mutex.Lock()
	defer e.mutex.Unlock()
	return e.running
}

func (e *Elevator) CurrentFloor() int {
	e.mutex.Lock()
	defer e.mutex.Unlock()
	return e.currentFloor
}

func (e *Elevator) IsIdle() bool {
	e.mutex.Lock()
	defer e.mutex.Unlock()
	return e.direction == DirectionIdle && e.upStops.IsEmpty() && e.downStops.IsEmpty()
}

func (e *Elevator) AddRequest(floor int) {
	e.mutex.Lock()
	defer e.mutex.Unlock()
	if floor > e.currentFloor {
		e.upStops.Add(floor)
		if e.direction == DirectionIdle {
			e.direction = Up
		}
	} else if floor < e.currentFloor {
		e.downStops.Add(floor)
		if e.direction == DirectionIdle {
			e.direction = Down
		}
	}
	// floor == e.currentFloor: already there, nothing to queue.
}

func (e *Elevator) Run() {
	for e.isRunning() {
		e.Step()
		time.Sleep(50 * time.Millisecond) // simulate the time to travel one floor
	}
}

// Step advances the elevator by exactly one unit of work: either it opens
// its doors for a stop it has just reached, or it moves one floor toward
// the next stop, or -- with nothing left to do -- it goes idle.
func (e *Elevator) Step() {
	e.mutex.Lock()
	defer e.mutex.Unlock()

	switch e.direction {
	case Up:
		if e.upStops.IsEmpty() {
			if e.downStops.IsEmpty() {
				e.direction = DirectionIdle
				e.state = StateIdle
			} else {
				e.direction = Down
				e.state = Moving
			}
			return
		}
		if e.currentFloor == e.upStops.First() {
			e.upStops.PollFirst()
			e.openAndCloseDoors()
			return
		}
		e.state = Moving
		e.currentFloor++
	case Down:
		if e.downStops.IsEmpty() {
			if e.upStops.IsEmpty() {
				e.direction = DirectionIdle
				e.state = StateIdle
			} else {
				e.direction = Up
				e.state = Moving
			}
			return
		}
		if e.currentFloor == e.downStops.Last() {
			e.downStops.PollLast()
			e.openAndCloseDoors()
			return
		}
		e.state = Moving
		e.currentFloor--
	default:
		e.state = StateIdle
	}
}

// Called with e.mutex already held.
func (e *Elevator) openAndCloseDoors() {
	e.state = Stopped
	e.doorState = Open
	printLine(fmt.Sprintf("Elevator %d arrived at floor %d, doors OPEN", e.ID, e.currentFloor))
	e.doorState = Closed
	printLine(fmt.Sprintf("Elevator %d doors CLOSED at floor %d", e.ID, e.currentFloor))
}

// printMutex serializes stdout writes: each elevator runs on its own
// goroutine, so without this, two Printf calls firing at once could
// interleave their output mid-line.
var printMutex sync.Mutex

func printLine(line string) {
	printMutex.Lock()
	defer printMutex.Unlock()
	fmt.Println(line)
}

type ElevatorSystem struct {
	elevators []*Elevator
	wg        sync.WaitGroup
	strategy  SchedulingStrategy
}

func NewElevatorSystem(numberOfElevators int, strategy SchedulingStrategy) *ElevatorSystem {
	system := &ElevatorSystem{strategy: strategy}
	for i := 1; i <= numberOfElevators; i++ {
		elevator := NewElevator(i, 0)
		elevator.StartElevator()
		system.elevators = append(system.elevators, elevator)
		system.wg.Add(1)
		go func() {
			defer system.wg.Done()
			elevator.Run()
		}()
	}
	return system
}

func (s *ElevatorSystem) AddRequest(request *Request) {
	elevator := s.strategy.GetElevator(s.elevators, request)
	elevator.AddRequest(request.Floor)
	printLine(fmt.Sprintf("Assigned request for floor %d to elevator %d", request.Floor, elevator.ID))
}

// AwaitAllIdle avoids busy-polling on the caller's side too: sleep between
// checks instead of spinning, and give up after timeout so a bug in one
// elevator can't hang the demo forever.
func (s *ElevatorSystem) AwaitAllIdle(timeout time.Duration) bool {
	deadline := time.Now().Add(timeout)
	for time.Now().Before(deadline) {
		allIdle := true
		for _, elevator := range s.elevators {
			if !elevator.IsIdle() {
				allIdle = false
				break
			}
		}
		if allIdle {
			return true
		}
		time.Sleep(20 * time.Millisecond)
	}
	return false
}

func (s *ElevatorSystem) PrintFleetStatus() {
	for _, elevator := range s.elevators {
		printLine(fmt.Sprintf("  Elevator %d resting at floor %d", elevator.ID, elevator.CurrentFloor()))
	}
}

func (s *ElevatorSystem) Shutdown() {
	for _, elevator := range s.elevators {
		elevator.StopElevator()
	}
	s.wg.Wait()
}

func main() {
	system := NewElevatorSystem(3, &NearestElevatorStrategy{})

	system.AddRequest(&Request{Floor: 5, Direction: Up})
	system.AddRequest(&Request{Floor: 2, Direction: Up})
	system.AddRequest(&Request{Floor: 8, Direction: Down})
	system.AddRequest(&Request{Floor: 1, Direction: Down})

	finished := system.AwaitAllIdle(5 * time.Second)
	printLine(fmt.Sprintf("\nAll requests serviced: %t", finished))
	printLine("Final fleet status:")
	system.PrintFleetStatus()

	system.Shutdown()
}
