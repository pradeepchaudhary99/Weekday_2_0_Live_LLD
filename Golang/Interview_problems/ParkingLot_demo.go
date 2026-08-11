/*
================================================================================
LLD: Parking Lot
================================================================================

Functional Requirements:
    1. ParkingLot should be able to manage multiple floors.
    2. Every floor can have any number of parking spots.
    3. ParkingLot should support different sizes/types of vehicles and spots.
    4. ParkingLot should support multiple entry and exit gates.
    5. Find available parking spots.
    6. Support ticket generation at the entry of the vehicle.
    7. Support payment strategies at the exit of the vehicle.

Non-Functional Requirements:
    1. Thread-safety when multiple gates park/unpark concurrently.
    2. Maintainability / extensibility to new vehicle types, spot types,
       pricing, and payment strategies.

Design:
    ParkingSpotAllocationStrategy (Strategy) decides which free spot a
    vehicle gets; NearestAvailableSpotStrategy scans floors in order and
    returns the first spot the vehicle fits in.

    PriceCalculationStrategy (Strategy) turns a parked duration into a fee;
    HourlyRateStrategy charges a per-vehicle-type hourly rate, rounded up to
    the next full hour with a one-hour minimum.

    PaymentStrategy (Strategy) is the pluggable checkout channel (cash,
    card, ...).

    EntryGate issues a Ticket by asking the allocation strategy for a spot
    and reserving it. ExitGate closes a Ticket by pricing the elapsed time
    and charging it through a PaymentStrategy, then frees the spot.

Core Entities:
    ParkingLot (manager)
    ParkingFloor
    ParkingSpot
    Vehicle (Bike / Car / Truck)
    Ticket
    ParkingSpotAllocationStrategy / NearestAvailableSpotStrategy
    PriceCalculationStrategy / HourlyRateStrategy
    PaymentStrategy / CashPayment / CardPayment
    EntryGate
    ExitGate
================================================================================
*/

package main

import (
	"errors"
	"fmt"
	"math"
	"sync"
	"time"
)

type VehicleType int

const (
	Bike VehicleType = iota
	Car
	Truck
)

func (t VehicleType) String() string {
	switch t {
	case Bike:
		return "BIKE"
	case Car:
		return "CAR"
	case Truck:
		return "TRUCK"
	}
	return "UNKNOWN"
}

type SpotType int

const (
	SpotBike SpotType = iota
	SpotCompact
	SpotLarge
)

type TicketStatus int

const (
	Active TicketStatus = iota
	Paid
	Closed
)

// spotFits reports whether a spot's category is large enough for the vehicle type.
func spotFits(spotType SpotType, vehicleType VehicleType) bool {
	switch spotType {
	case SpotBike:
		return vehicleType == Bike
	case SpotCompact:
		return vehicleType == Car
	case SpotLarge:
		return vehicleType == Car || vehicleType == Truck
	}
	return false
}

type Vehicle struct {
	LicenseNumber string
	Type          VehicleType
}

type ParkingSpot struct {
	ID          string
	FloorNumber int
	Type        SpotType
	Vehicle     *Vehicle
}

func (s *ParkingSpot) IsOccupied() bool                    { return s.Vehicle != nil }
func (s *ParkingSpot) CanFit(vehicleType VehicleType) bool { return spotFits(s.Type, vehicleType) }
func (s *ParkingSpot) Park(v *Vehicle)                     { s.Vehicle = v }
func (s *ParkingSpot) Vacate()                             { s.Vehicle = nil }

type ParkingFloor struct {
	FloorNumber int
	Spots       []*ParkingSpot
}

type ParkingSpotAllocationStrategy interface {
	FindSpot(floors []*ParkingFloor, vehicleType VehicleType) *ParkingSpot
}

type NearestAvailableSpotStrategy struct{}

func (NearestAvailableSpotStrategy) FindSpot(floors []*ParkingFloor, vehicleType VehicleType) *ParkingSpot {
	for _, floor := range floors {
		for _, spot := range floor.Spots {
			if !spot.IsOccupied() && spot.CanFit(vehicleType) {
				return spot
			}
		}
	}
	return nil
}

type Ticket struct {
	ID        string
	Vehicle   *Vehicle
	Spot      *ParkingSpot
	EntryTime time.Time
	ExitTime  time.Time
	Amount    float64
	Status    TicketStatus
}

type PriceCalculationStrategy interface {
	CalculateFee(ticket *Ticket) float64
}

type HourlyRateStrategy struct{}

func (HourlyRateStrategy) CalculateFee(ticket *Ticket) float64 {
	durationHours := ticket.ExitTime.Sub(ticket.EntryTime).Hours()
	hours := int(math.Ceil(durationHours))
	if hours < 1 {
		hours = 1
	}
	return float64(hours) * hourlyRate(ticket.Vehicle.Type)
}

func hourlyRate(vehicleType VehicleType) float64 {
	switch vehicleType {
	case Bike:
		return 10.0
	case Car:
		return 20.0
	case Truck:
		return 30.0
	}
	return 0.0
}

type PaymentStrategy interface {
	Pay(amount float64) bool
}

type CashPayment struct{}

func (CashPayment) Pay(amount float64) bool {
	fmt.Printf("  Paid $%.2f in cash\n", amount)
	return true
}

type CardPayment struct{}

func (CardPayment) Pay(amount float64) bool {
	fmt.Printf("  Paid $%.2f by card\n", amount)
	return true
}

var ErrParkingLotFull = errors.New("no available spot")

type ParkingLot struct {
	mu                 sync.Mutex
	floors             []*ParkingFloor
	allocationStrategy ParkingSpotAllocationStrategy
	priceStrategy      PriceCalculationStrategy
	nextTicketID       int
}

func NewParkingLot(floors []*ParkingFloor, allocationStrategy ParkingSpotAllocationStrategy,
	priceStrategy PriceCalculationStrategy) *ParkingLot {
	return &ParkingLot{floors: floors, allocationStrategy: allocationStrategy,
		priceStrategy: priceStrategy, nextTicketID: 1}
}

func (l *ParkingLot) ParkVehicle(vehicle *Vehicle, entryTime time.Time) (*Ticket, error) {
	l.mu.Lock()
	defer l.mu.Unlock()
	spot := l.allocationStrategy.FindSpot(l.floors, vehicle.Type)
	if spot == nil {
		return nil, fmt.Errorf("%w for vehicle type %s", ErrParkingLotFull, vehicle.Type)
	}
	spot.Park(vehicle)
	ticket := &Ticket{ID: fmt.Sprintf("T%d", l.nextTicketID), Vehicle: vehicle, Spot: spot,
		EntryTime: entryTime, Status: Active}
	l.nextTicketID++
	return ticket, nil
}

func (l *ParkingLot) UnparkVehicle(ticket *Ticket, paymentStrategy PaymentStrategy, exitTime time.Time) (float64, error) {
	if ticket.Status != Active {
		return 0, fmt.Errorf("ticket %s is not active", ticket.ID)
	}
	ticket.ExitTime = exitTime
	ticket.Amount = l.priceStrategy.CalculateFee(ticket)
	paymentStrategy.Pay(ticket.Amount)
	ticket.Status = Paid
	ticket.Spot.Vacate()
	ticket.Status = Closed
	return ticket.Amount, nil
}

type EntryGate struct {
	ID         string
	ParkingLot *ParkingLot
}

func (g *EntryGate) IssueTicket(vehicle *Vehicle, entryTime time.Time) (*Ticket, error) {
	ticket, err := g.ParkingLot.ParkVehicle(vehicle, entryTime)
	if err != nil {
		return nil, err
	}
	fmt.Printf("[%s] Issued %s for %s -> spot %s\n", g.ID, ticket.ID, vehicle.LicenseNumber, ticket.Spot.ID)
	return ticket, nil
}

type ExitGate struct {
	ID         string
	ParkingLot *ParkingLot
}

func (g *ExitGate) ProcessExit(ticket *Ticket, paymentStrategy PaymentStrategy, exitTime time.Time) (float64, error) {
	fmt.Printf("[%s] Closing %s for %s\n", g.ID, ticket.ID, ticket.Vehicle.LicenseNumber)
	amount, err := g.ParkingLot.UnparkVehicle(ticket, paymentStrategy, exitTime)
	if err != nil {
		return 0, err
	}
	fmt.Printf("  Freed spot %s\n", ticket.Spot.ID)
	return amount, nil
}

func buildFloor(floorNumber, bikeSpots, compactSpots, largeSpots int) *ParkingFloor {
	var spots []*ParkingSpot
	for i := 1; i <= bikeSpots; i++ {
		spots = append(spots, &ParkingSpot{ID: fmt.Sprintf("F%d-B%d", floorNumber, i), FloorNumber: floorNumber, Type: SpotBike})
	}
	for i := 1; i <= compactSpots; i++ {
		spots = append(spots, &ParkingSpot{ID: fmt.Sprintf("F%d-C%d", floorNumber, i), FloorNumber: floorNumber, Type: SpotCompact})
	}
	for i := 1; i <= largeSpots; i++ {
		spots = append(spots, &ParkingSpot{ID: fmt.Sprintf("F%d-L%d", floorNumber, i), FloorNumber: floorNumber, Type: SpotLarge})
	}
	return &ParkingFloor{FloorNumber: floorNumber, Spots: spots}
}

func main() {
	floors := []*ParkingFloor{buildFloor(1, 1, 1, 1)}
	lot := NewParkingLot(floors, NearestAvailableSpotStrategy{}, HourlyRateStrategy{})
	entryGate := &EntryGate{ID: "Entry-1", ParkingLot: lot}
	exitGate := &ExitGate{ID: "Exit-1", ParkingLot: lot}

	now := time.Now()

	bike := &Vehicle{LicenseNumber: "KA-01-1234", Type: Bike}
	car := &Vehicle{LicenseNumber: "KA-02-5678", Type: Car}
	truck := &Vehicle{LicenseNumber: "KA-03-9999", Type: Truck}

	bikeTicket, _ := entryGate.IssueTicket(bike, now.Add(-30*time.Minute))
	carTicket, _ := entryGate.IssueTicket(car, now.Add(-135*time.Minute))
	truckTicket, _ := entryGate.IssueTicket(truck, now.Add(-1*time.Hour))

	fmt.Println("\nAll compact/large spots are now full -- another car must wait:")
	secondCar := &Vehicle{LicenseNumber: "KA-04-1111", Type: Car}
	if _, err := entryGate.IssueTicket(secondCar, now); err != nil {
		fmt.Printf("  %s\n", err)
	}

	fmt.Println("\nProcessing exits:")
	exitGate.ProcessExit(bikeTicket, CashPayment{}, now)
	exitGate.ProcessExit(carTicket, CardPayment{}, now)
	exitGate.ProcessExit(truckTicket, CashPayment{}, now)

	fmt.Println("\nSpot now free -- the waiting car can park:")
	entryGate.IssueTicket(secondCar, now)
}
