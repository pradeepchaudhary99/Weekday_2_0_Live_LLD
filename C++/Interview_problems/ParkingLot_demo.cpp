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

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

enum class VehicleType { BIKE, CAR, TRUCK };
enum class SpotType { BIKE, COMPACT, LARGE };
enum class TicketStatus { ACTIVE, PAID, CLOSED };

using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;

bool spotFits(SpotType spotType, VehicleType vehicleType) {
    switch (spotType) {
        case SpotType::BIKE: return vehicleType == VehicleType::BIKE;
        case SpotType::COMPACT: return vehicleType == VehicleType::CAR;
        case SpotType::LARGE: return vehicleType == VehicleType::CAR || vehicleType == VehicleType::TRUCK;
    }
    return false;
}

std::string vehicleTypeName(VehicleType type) {
    switch (type) {
        case VehicleType::BIKE: return "BIKE";
        case VehicleType::CAR: return "CAR";
        case VehicleType::TRUCK: return "TRUCK";
    }
    return "UNKNOWN";
}

struct Vehicle {
    std::string licenseNumber;
    VehicleType type;
};

class ParkingSpot {
public:
    std::string id;
    int floorNumber;
    SpotType type;
    std::shared_ptr<Vehicle> vehicle;

    ParkingSpot(std::string id, int floorNumber, SpotType type)
        : id(std::move(id)), floorNumber(floorNumber), type(type) {}

    bool isOccupied() const { return vehicle != nullptr; }
    bool canFit(VehicleType vehicleType) const { return spotFits(type, vehicleType); }
    void park(std::shared_ptr<Vehicle> v) { vehicle = std::move(v); }
    void vacate() { vehicle = nullptr; }
};

struct ParkingFloor {
    int floorNumber;
    std::vector<std::shared_ptr<ParkingSpot>> spots;
};

struct ParkingSpotAllocationStrategy {
    virtual ~ParkingSpotAllocationStrategy() = default;
    virtual std::shared_ptr<ParkingSpot> findSpot(const std::vector<ParkingFloor>& floors,
                                                    VehicleType vehicleType) const = 0;
};

class NearestAvailableSpotStrategy : public ParkingSpotAllocationStrategy {
public:
    std::shared_ptr<ParkingSpot> findSpot(const std::vector<ParkingFloor>& floors,
                                           VehicleType vehicleType) const override {
        for (const auto& floor : floors) {
            for (const auto& spot : floor.spots) {
                if (!spot->isOccupied() && spot->canFit(vehicleType)) {
                    return spot;
                }
            }
        }
        return nullptr;
    }
};

struct Ticket {
    std::string id;
    std::shared_ptr<Vehicle> vehicle;
    std::shared_ptr<ParkingSpot> spot;
    TimePoint entryTime;
    TimePoint exitTime;
    double amount = 0.0;
    TicketStatus status = TicketStatus::ACTIVE;
};

struct PriceCalculationStrategy {
    virtual ~PriceCalculationStrategy() = default;
    virtual double calculateFee(const Ticket& ticket) const = 0;
};

class HourlyRateStrategy : public PriceCalculationStrategy {
public:
    double calculateFee(const Ticket& ticket) const override {
        double seconds = std::chrono::duration<double>(ticket.exitTime - ticket.entryTime).count();
        double durationHours = seconds / 3600.0;
        int hours = std::max(1, static_cast<int>(std::ceil(durationHours)));
        return hours * rate(ticket.vehicle->type);
    }

private:
    static double rate(VehicleType type) {
        switch (type) {
            case VehicleType::BIKE: return 10.0;
            case VehicleType::CAR: return 20.0;
            case VehicleType::TRUCK: return 30.0;
        }
        return 0.0;
    }
};

struct PaymentStrategy {
    virtual ~PaymentStrategy() = default;
    virtual bool pay(double amount) const = 0;
};

class CashPayment : public PaymentStrategy {
public:
    bool pay(double amount) const override {
        std::printf("  Paid $%.2f in cash\n", amount);
        return true;
    }
};

class CardPayment : public PaymentStrategy {
public:
    bool pay(double amount) const override {
        std::printf("  Paid $%.2f by card\n", amount);
        return true;
    }
};

class ParkingLotFullError : public std::runtime_error {
public:
    explicit ParkingLotFullError(const std::string& message) : std::runtime_error(message) {}
};

class ParkingLot {
public:
    ParkingLot(std::vector<ParkingFloor> floors, std::shared_ptr<ParkingSpotAllocationStrategy> allocationStrategy,
               std::shared_ptr<PriceCalculationStrategy> priceStrategy)
        : floors_(std::move(floors)), allocationStrategy_(std::move(allocationStrategy)),
          priceStrategy_(std::move(priceStrategy)) {}

    std::shared_ptr<Ticket> parkVehicle(std::shared_ptr<Vehicle> vehicle, TimePoint entryTime) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto spot = allocationStrategy_->findSpot(floors_, vehicle->type);
        if (!spot) {
            throw ParkingLotFullError("No available spot for vehicle type " + vehicleTypeName(vehicle->type));
        }
        spot->park(vehicle);
        auto ticket = std::make_shared<Ticket>();
        ticket->id = "T" + std::to_string(nextTicketId_++);
        ticket->vehicle = vehicle;
        ticket->spot = spot;
        ticket->entryTime = entryTime;
        return ticket;
    }

    double unparkVehicle(std::shared_ptr<Ticket> ticket, const PaymentStrategy& paymentStrategy, TimePoint exitTime) {
        if (ticket->status != TicketStatus::ACTIVE) {
            throw std::invalid_argument("Ticket " + ticket->id + " is not active");
        }
        ticket->exitTime = exitTime;
        ticket->amount = priceStrategy_->calculateFee(*ticket);
        paymentStrategy.pay(ticket->amount);
        ticket->status = TicketStatus::PAID;
        ticket->spot->vacate();
        ticket->status = TicketStatus::CLOSED;
        return ticket->amount;
    }

private:
    std::vector<ParkingFloor> floors_;
    std::shared_ptr<ParkingSpotAllocationStrategy> allocationStrategy_;
    std::shared_ptr<PriceCalculationStrategy> priceStrategy_;
    std::mutex mutex_;
    int nextTicketId_ = 1;
};

class EntryGate {
public:
    EntryGate(std::string id, std::shared_ptr<ParkingLot> parkingLot)
        : id_(std::move(id)), parkingLot_(std::move(parkingLot)) {}

    std::shared_ptr<Ticket> issueTicket(std::shared_ptr<Vehicle> vehicle, TimePoint entryTime) {
        auto ticket = parkingLot_->parkVehicle(vehicle, entryTime);
        std::cout << "[" << id_ << "] Issued " << ticket->id << " for " << vehicle->licenseNumber
                  << " -> spot " << ticket->spot->id << "\n";
        return ticket;
    }

private:
    std::string id_;
    std::shared_ptr<ParkingLot> parkingLot_;
};

class ExitGate {
public:
    ExitGate(std::string id, std::shared_ptr<ParkingLot> parkingLot)
        : id_(std::move(id)), parkingLot_(std::move(parkingLot)) {}

    double processExit(std::shared_ptr<Ticket> ticket, const PaymentStrategy& paymentStrategy, TimePoint exitTime) {
        std::cout << "[" << id_ << "] Closing " << ticket->id << " for " << ticket->vehicle->licenseNumber << "\n";
        double amount = parkingLot_->unparkVehicle(ticket, paymentStrategy, exitTime);
        std::cout << "  Freed spot " << ticket->spot->id << "\n";
        return amount;
    }

private:
    std::string id_;
    std::shared_ptr<ParkingLot> parkingLot_;
};

ParkingFloor buildFloor(int floorNumber, int bikeSpots, int compactSpots, int largeSpots) {
    ParkingFloor floor{floorNumber, {}};
    for (int i = 1; i <= bikeSpots; ++i) {
        floor.spots.push_back(std::make_shared<ParkingSpot>(
            "F" + std::to_string(floorNumber) + "-B" + std::to_string(i), floorNumber, SpotType::BIKE));
    }
    for (int i = 1; i <= compactSpots; ++i) {
        floor.spots.push_back(std::make_shared<ParkingSpot>(
            "F" + std::to_string(floorNumber) + "-C" + std::to_string(i), floorNumber, SpotType::COMPACT));
    }
    for (int i = 1; i <= largeSpots; ++i) {
        floor.spots.push_back(std::make_shared<ParkingSpot>(
            "F" + std::to_string(floorNumber) + "-L" + std::to_string(i), floorNumber, SpotType::LARGE));
    }
    return floor;
}

int main() {
    std::vector<ParkingFloor> floors = {buildFloor(1, 1, 1, 1)};
    auto lot = std::make_shared<ParkingLot>(floors, std::make_shared<NearestAvailableSpotStrategy>(),
                                             std::make_shared<HourlyRateStrategy>());
    EntryGate entryGate("Entry-1", lot);
    ExitGate exitGate("Exit-1", lot);

    TimePoint now = Clock::now();

    auto bike = std::make_shared<Vehicle>(Vehicle{"KA-01-1234", VehicleType::BIKE});
    auto car = std::make_shared<Vehicle>(Vehicle{"KA-02-5678", VehicleType::CAR});
    auto truck = std::make_shared<Vehicle>(Vehicle{"KA-03-9999", VehicleType::TRUCK});

    auto bikeTicket = entryGate.issueTicket(bike, now - std::chrono::minutes(30));
    auto carTicket = entryGate.issueTicket(car, now - std::chrono::minutes(135));
    auto truckTicket = entryGate.issueTicket(truck, now - std::chrono::hours(1));

    std::cout << "\nAll compact/large spots are now full -- another car must wait:\n";
    auto secondCar = std::make_shared<Vehicle>(Vehicle{"KA-04-1111", VehicleType::CAR});
    try {
        entryGate.issueTicket(secondCar, now);
    } catch (const ParkingLotFullError& e) {
        std::cout << "  " << e.what() << "\n";
    }

    std::cout << "\nProcessing exits:\n";
    CashPayment cash;
    CardPayment card;
    exitGate.processExit(bikeTicket, cash, now);
    exitGate.processExit(carTicket, card, now);
    exitGate.processExit(truckTicket, cash, now);

    std::cout << "\nSpot now free -- the waiting car can park:\n";
    entryGate.issueTicket(secondCar, now);

    return 0;
}
