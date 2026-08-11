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
    1. Thread-safety (moot for Node's single-threaded event loop, but the
       design would carry over to a worker-thread model).
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

const VehicleType = Object.freeze({ BIKE: "BIKE", CAR: "CAR", TRUCK: "TRUCK" });
const SpotType = Object.freeze({ BIKE: "BIKE", COMPACT: "COMPACT", LARGE: "LARGE" });
const TicketStatus = Object.freeze({ ACTIVE: "ACTIVE", PAID: "PAID", CLOSED: "CLOSED" });

// A spot can host a vehicle type if the spot's category is large enough.
const SPOT_FITS = {
    [SpotType.BIKE]: [VehicleType.BIKE],
    [SpotType.COMPACT]: [VehicleType.CAR],
    [SpotType.LARGE]: [VehicleType.CAR, VehicleType.TRUCK],
};

class Vehicle {
    constructor(licenseNumber, type) {
        this.licenseNumber = licenseNumber;
        this.type = type;
    }
}

class ParkingSpot {
    constructor(id, floorNumber, type) {
        this.id = id;
        this.floorNumber = floorNumber;
        this.type = type;
        this.vehicle = null;
    }

    isOccupied() {
        return this.vehicle !== null;
    }

    canFit(vehicleType) {
        return SPOT_FITS[this.type].includes(vehicleType);
    }

    park(vehicle) {
        this.vehicle = vehicle;
    }

    vacate() {
        this.vehicle = null;
    }
}

class ParkingFloor {
    constructor(floorNumber, spots) {
        this.floorNumber = floorNumber;
        this.spots = spots;
    }
}

class ParkingSpotAllocationStrategy {
    findSpot(_floors, _vehicleType) {
        throw new Error("findSpot must be implemented by subclasses");
    }
}

class NearestAvailableSpotStrategy extends ParkingSpotAllocationStrategy {
    findSpot(floors, vehicleType) {
        for (const floor of floors) {
            for (const spot of floor.spots) {
                if (!spot.isOccupied() && spot.canFit(vehicleType)) {
                    return spot;
                }
            }
        }
        return null;
    }
}

class Ticket {
    constructor(id, vehicle, spot, entryTime) {
        this.id = id;
        this.vehicle = vehicle;
        this.spot = spot;
        this.entryTime = entryTime;
        this.exitTime = null;
        this.amount = 0.0;
        this.status = TicketStatus.ACTIVE;
    }
}

class PriceCalculationStrategy {
    calculateFee(_ticket) {
        throw new Error("calculateFee must be implemented by subclasses");
    }
}

class HourlyRateStrategy extends PriceCalculationStrategy {
    static RATES = {
        [VehicleType.BIKE]: 10.0,
        [VehicleType.CAR]: 20.0,
        [VehicleType.TRUCK]: 30.0,
    };

    calculateFee(ticket) {
        const durationHours = (ticket.exitTime - ticket.entryTime) / (1000 * 60 * 60);
        const hours = Math.max(1, Math.ceil(durationHours));
        return hours * HourlyRateStrategy.RATES[ticket.vehicle.type];
    }
}

class PaymentStrategy {
    pay(_amount) {
        throw new Error("pay must be implemented by subclasses");
    }
}

class CashPayment extends PaymentStrategy {
    pay(amount) {
        console.log(`  Paid $${amount.toFixed(2)} in cash`);
        return true;
    }
}

class CardPayment extends PaymentStrategy {
    pay(amount) {
        console.log(`  Paid $${amount.toFixed(2)} by card`);
        return true;
    }
}

class ParkingLotFullError extends Error {}

class ParkingLot {
    constructor(floors, allocationStrategy, priceStrategy) {
        this.floors = floors;
        this.allocationStrategy = allocationStrategy;
        this.priceStrategy = priceStrategy;
        this._nextTicketId = 1;
    }

    parkVehicle(vehicle, entryTime) {
        const spot = this.allocationStrategy.findSpot(this.floors, vehicle.type);
        if (!spot) {
            throw new ParkingLotFullError(`No available spot for vehicle type ${vehicle.type}`);
        }
        spot.park(vehicle);
        const ticket = new Ticket(`T${this._nextTicketId++}`, vehicle, spot, entryTime);
        return ticket;
    }

    unparkVehicle(ticket, paymentStrategy, exitTime) {
        if (ticket.status !== TicketStatus.ACTIVE) {
            throw new Error(`Ticket ${ticket.id} is not active`);
        }
        ticket.exitTime = exitTime;
        ticket.amount = this.priceStrategy.calculateFee(ticket);
        paymentStrategy.pay(ticket.amount);
        ticket.status = TicketStatus.PAID;
        ticket.spot.vacate();
        ticket.status = TicketStatus.CLOSED;
        return ticket.amount;
    }
}

class EntryGate {
    constructor(id, parkingLot) {
        this.id = id;
        this.parkingLot = parkingLot;
    }

    issueTicket(vehicle, entryTime) {
        const ticket = this.parkingLot.parkVehicle(vehicle, entryTime);
        console.log(`[${this.id}] Issued ${ticket.id} for ${vehicle.licenseNumber} -> spot ${ticket.spot.id}`);
        return ticket;
    }
}

class ExitGate {
    constructor(id, parkingLot) {
        this.id = id;
        this.parkingLot = parkingLot;
    }

    processExit(ticket, paymentStrategy, exitTime) {
        console.log(`[${this.id}] Closing ${ticket.id} for ${ticket.vehicle.licenseNumber}`);
        const amount = this.parkingLot.unparkVehicle(ticket, paymentStrategy, exitTime);
        console.log(`  Freed spot ${ticket.spot.id}`);
        return amount;
    }
}

function buildFloor(floorNumber, bikeSpots, compactSpots, largeSpots) {
    const spots = [];
    for (let i = 1; i <= bikeSpots; i++) {
        spots.push(new ParkingSpot(`F${floorNumber}-B${i}`, floorNumber, SpotType.BIKE));
    }
    for (let i = 1; i <= compactSpots; i++) {
        spots.push(new ParkingSpot(`F${floorNumber}-C${i}`, floorNumber, SpotType.COMPACT));
    }
    for (let i = 1; i <= largeSpots; i++) {
        spots.push(new ParkingSpot(`F${floorNumber}-L${i}`, floorNumber, SpotType.LARGE));
    }
    return new ParkingFloor(floorNumber, spots);
}

function main() {
    const floors = [buildFloor(1, 1, 1, 1)];
    const lot = new ParkingLot(floors, new NearestAvailableSpotStrategy(), new HourlyRateStrategy());
    const entryGate = new EntryGate("Entry-1", lot);
    const exitGate = new ExitGate("Exit-1", lot);

    const now = Date.now();
    const MINUTE = 60 * 1000;
    const HOUR = 60 * MINUTE;

    const bike = new Vehicle("KA-01-1234", VehicleType.BIKE);
    const car = new Vehicle("KA-02-5678", VehicleType.CAR);
    const truck = new Vehicle("KA-03-9999", VehicleType.TRUCK);

    const bikeTicket = entryGate.issueTicket(bike, now - 30 * MINUTE);
    const carTicket = entryGate.issueTicket(car, now - (2 * HOUR + 15 * MINUTE));
    const truckTicket = entryGate.issueTicket(truck, now - HOUR);

    console.log("\nAll compact/large spots are now full -- another car must wait:");
    const secondCar = new Vehicle("KA-04-1111", VehicleType.CAR);
    try {
        entryGate.issueTicket(secondCar, now);
    } catch (e) {
        if (e instanceof ParkingLotFullError) {
            console.log(`  ${e.message}`);
        } else {
            throw e;
        }
    }

    console.log("\nProcessing exits:");
    exitGate.processExit(bikeTicket, new CashPayment(), now);
    exitGate.processExit(carTicket, new CardPayment(), now);
    exitGate.processExit(truckTicket, new CashPayment(), now);

    console.log("\nSpot now free -- the waiting car can park:");
    entryGate.issueTicket(secondCar, now);
}

main();
