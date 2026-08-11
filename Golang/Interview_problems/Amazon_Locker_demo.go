/*
================================================================================
LLD: Amazon Locker
================================================================================

Functional Requirements:
    1. Register a package for delivery into a locker sized to fit it
       (a delivery agent drops the package into an assigned locker).
    2. Find a suitable free locker for a package via a pluggable
       allocation strategy.
    3. Generate a pickup code when a package is stored (OTP / QR).
    4. Let a user claim (pick up) a package by presenting the pickup code.
    5. Query package/locker status.

Non-Functional Requirements:
    1. Thread-safety.
    2. Maintainability / extensibility to new allocation and pickup-code
       strategies.
    3. Performance: locker lookup should not scan the whole system for
       every request.

Design:
    LockerAllocationStrategy (Strategy) picks a free locker for a
    package's size; SmallestFitAllocationStrategy returns the smallest
    available locker that is still big enough for the package, so large
    lockers stay free for large packages.

    PickupCodeGenerationStrategy (Strategy) produces the code a user must
    present at pickup; OtpPickupCodeStrategy generates a random 6-digit
    numeric OTP.

    LockerSystemManager owns the lockers and answers allocation queries.
    AmazonLockerManager (facade) wires the system manager and the two
    strategies together: RegisterPackage() finds+reserves a locker,
    generates a code, and stores the package; ClaimPackage() validates
    the presented code against the reserved locker and releases it.

Core Entities:
    Size, LockerStatus, PackageStatus (enums)
    Package
    Locker
    LockerAllocationStrategy / SmallestFitAllocationStrategy
    PickupCodeGenerationStrategy / OtpPickupCodeStrategy
    LockerSystemManager
    AmazonLockerManager
================================================================================
*/

package main

import (
	"errors"
	"fmt"
	"math/rand"
	"sync"
)

type Size int

const (
	Small Size = iota + 1
	Medium
	Large
)

func (s Size) String() string {
	switch s {
	case Small:
		return "SMALL"
	case Medium:
		return "MEDIUM"
	case Large:
		return "LARGE"
	}
	return "UNKNOWN"
}

type LockerStatus int

const (
	Available LockerStatus = iota
	Reserved
	Occupied
	OutOfService
)

func (s LockerStatus) String() string {
	switch s {
	case Available:
		return "AVAILABLE"
	case Reserved:
		return "RESERVED"
	case Occupied:
		return "OCCUPIED"
	case OutOfService:
		return "OUT_OF_SERVICE"
	}
	return "UNKNOWN"
}

type PackageStatus int

const (
	Created PackageStatus = iota
	Assigned
	Delivered
	PickedUp
	Expired
	Returned
)

func (s PackageStatus) String() string {
	switch s {
	case Created:
		return "CREATED"
	case Assigned:
		return "ASSIGNED"
	case Delivered:
		return "DELIVERED"
	case PickedUp:
		return "PICKED_UP"
	case Expired:
		return "EXPIRED"
	case Returned:
		return "RETURNED"
	}
	return "UNKNOWN"
}

type Package struct {
	ID         string
	Size       Size
	Address    string
	Status     PackageStatus
	PickupCode string
}

type Locker struct {
	ID             string
	Size           Size
	Status         LockerStatus
	CurrentPackage *Package
	pickupCode     string
}

func (l *Locker) CanFit(pkg *Package) bool {
	return l.Status == Available && l.Size >= pkg.Size
}

func (l *Locker) StorePackage(pkg *Package, pickupCode string) {
	l.CurrentPackage = pkg
	l.pickupCode = pickupCode
	l.Status = Occupied
	pkg.Status = Delivered
	pkg.PickupCode = pickupCode
}

func (l *Locker) Pickup(pickupCode string) *Package {
	if l.CurrentPackage == nil || pickupCode != l.pickupCode {
		return nil
	}
	pkg := l.CurrentPackage
	pkg.Status = PickedUp
	l.CurrentPackage = nil
	l.pickupCode = ""
	l.Status = Available
	return pkg
}

type LockerAllocationStrategy interface {
	AssignLocker(lockers []*Locker, pkg *Package) *Locker
}

type SmallestFitAllocationStrategy struct{}

func (SmallestFitAllocationStrategy) AssignLocker(lockers []*Locker, pkg *Package) *Locker {
	var best *Locker
	for _, locker := range lockers {
		if !locker.CanFit(pkg) {
			continue
		}
		if best == nil || locker.Size < best.Size {
			best = locker
		}
	}
	return best
}

type PickupCodeGenerationStrategy interface {
	Generate() string
}

type OtpPickupCodeStrategy struct{}

func (OtpPickupCodeStrategy) Generate() string {
	return fmt.Sprintf("%06d", rand.Intn(1000000))
}

type LockerSystemManager struct {
	lockers []*Locker
	byID    map[string]*Locker
}

func NewLockerSystemManager() *LockerSystemManager {
	return &LockerSystemManager{byID: make(map[string]*Locker)}
}

func (m *LockerSystemManager) AddLocker(locker *Locker) {
	m.lockers = append(m.lockers, locker)
	m.byID[locker.ID] = locker
}

func (m *LockerSystemManager) AllLockers() []*Locker { return m.lockers }

func (m *LockerSystemManager) GetLocker(lockerID string) *Locker { return m.byID[lockerID] }

var (
	ErrNoLockerAvailable = errors.New("no locker available")
	ErrInvalidPickupCode = errors.New("invalid pickup code")
)

type AmazonLockerManager struct {
	mu                 sync.Mutex
	systemManager      *LockerSystemManager
	allocationStrategy LockerAllocationStrategy
	pickupCodeStrategy PickupCodeGenerationStrategy
	packages           map[string]*Package
}

func NewAmazonLockerManager(systemManager *LockerSystemManager, allocationStrategy LockerAllocationStrategy,
	pickupCodeStrategy PickupCodeGenerationStrategy) *AmazonLockerManager {
	return &AmazonLockerManager{systemManager: systemManager, allocationStrategy: allocationStrategy,
		pickupCodeStrategy: pickupCodeStrategy, packages: make(map[string]*Package)}
}

func (m *AmazonLockerManager) RegisterPackage(pkg *Package) (*Locker, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	locker := m.allocationStrategy.AssignLocker(m.systemManager.AllLockers(), pkg)
	if locker == nil {
		return nil, fmt.Errorf("%w for package %s (size %s)", ErrNoLockerAvailable, pkg.ID, pkg.Size)
	}
	pkg.Status = Assigned
	code := m.pickupCodeStrategy.Generate()
	locker.StorePackage(pkg, code)
	m.packages[pkg.ID] = pkg
	return locker, nil
}

func (m *AmazonLockerManager) ClaimPackage(lockerID, pickupCode string) (*Package, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	locker := m.systemManager.GetLocker(lockerID)
	if locker == nil {
		return nil, fmt.Errorf("unknown locker %s", lockerID)
	}
	pkg := locker.Pickup(pickupCode)
	if pkg == nil {
		return nil, fmt.Errorf("%w for locker %s", ErrInvalidPickupCode, lockerID)
	}
	return pkg, nil
}

func (m *AmazonLockerManager) PackageStatus(packageID string) (PackageStatus, bool) {
	pkg, ok := m.packages[packageID]
	if !ok {
		return 0, false
	}
	return pkg.Status, true
}

func (m *AmazonLockerManager) LockerStatus(lockerID string) (LockerStatus, bool) {
	locker := m.systemManager.GetLocker(lockerID)
	if locker == nil {
		return 0, false
	}
	return locker.Status, true
}

func main() {
	systemManager := NewLockerSystemManager()
	for i := 1; i <= 2; i++ {
		systemManager.AddLocker(&Locker{ID: fmt.Sprintf("S%d", i), Size: Small, Status: Available})
	}
	for i := 1; i <= 2; i++ {
		systemManager.AddLocker(&Locker{ID: fmt.Sprintf("M%d", i), Size: Medium, Status: Available})
	}
	systemManager.AddLocker(&Locker{ID: "L1", Size: Large, Status: Available})

	manager := NewAmazonLockerManager(systemManager, SmallestFitAllocationStrategy{}, OtpPickupCodeStrategy{})

	smallPkg := &Package{ID: "P1", Size: Small, Address: "221B Baker Street"}
	mediumPkg := &Package{ID: "P2", Size: Medium, Address: "42 Wallaby Way"}
	largePkg := &Package{ID: "P3", Size: Large, Address: "4 Privet Drive"}

	fmt.Println("Registering packages:")
	for _, pkg := range []*Package{smallPkg, mediumPkg, largePkg} {
		locker, _ := manager.RegisterPackage(pkg)
		fmt.Printf("  %s (%s) -> locker %s, pickup code %s\n", pkg.ID, pkg.Size, locker.ID, pkg.PickupCode)
	}

	fmt.Println("\nAttempting pickup with a wrong code:")
	if _, err := manager.ClaimPackage("S1", "000000"); err != nil {
		fmt.Printf("  %s\n", err)
	}

	fmt.Println("\nPicking up with the correct code:")
	claimed, _ := manager.ClaimPackage("S1", smallPkg.PickupCode)
	fmt.Printf("  Claimed %s, status now %s\n", claimed.ID, claimed.Status)

	s1Status, _ := manager.LockerStatus("S1")
	fmt.Printf("\nLocker S1 status: %s\n", s1Status)
	p2Status, _ := manager.PackageStatus(mediumPkg.ID)
	fmt.Printf("Package %s status: %s\n", mediumPkg.ID, p2Status)

	fmt.Println("\nA second small package can now reuse the freed locker:")
	anotherSmall := &Package{ID: "P4", Size: Small, Address: "12 Grimmauld Place"}
	locker, _ := manager.RegisterPackage(anotherSmall)
	fmt.Printf("  %s (%s) -> locker %s, pickup code %s\n", anotherSmall.ID, anotherSmall.Size, locker.ID, anotherSmall.PickupCode)
}
