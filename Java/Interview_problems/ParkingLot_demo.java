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

Core Entities:
    ParkingLotManager (singleton)
    ParkingFloor
    ParkingSpot
    Vehicle (Bike / Car / Truck)
    Ticket
    PaymentStrategy
    PriceCalculationStrategy
    EntryGate
    ExitGate

--------------------------------------------------------------------------
Design
--------------------------------------------------------------------------
State pattern for spot occupancy:
    ParkingSpot delegates parkVehicle()/unparkVehicle() to its current
    ParkingSpotState (NoVehicleState / HasVehicleState). Parking a vehicle
    in an already-occupied spot, or unparking an empty one, fails through
    the state itself instead of an isOccupied if-check scattered around
    the codebase.

Singleton for the manager:
    Exactly one ParkingLotManager coordinates all floors and gates, so
    every EntryGate/ExitGate allocation goes through one shared view of
    spot availability.
================================================================================
*/

import java.time.LocalDateTime;
import java.time.temporal.ChronoUnit;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.atomic.AtomicInteger;

enum VehicleType {
    BIKE, CAR, TRUCK
}

enum SpotType {
    BIKE, COMPACT, LARGE
}

enum TicketStatus {
    ACTIVE, PAID, CLOSED
}

class Vehicle {
    final String licenseNumber;
    final VehicleType type;

    Vehicle(String licenseNumber, VehicleType type) {
        this.licenseNumber = licenseNumber;
        this.type = type;
    }
}

class Bike extends Vehicle {
    Bike(String licenseNumber) {
        super(licenseNumber, VehicleType.BIKE);
    }
}

class Car extends Vehicle {
    Car(String licenseNumber) {
        super(licenseNumber, VehicleType.CAR);
    }
}

class Truck extends Vehicle {
    Truck(String licenseNumber) {
        super(licenseNumber, VehicleType.TRUCK);
    }
}

interface ParkingSpotState {
    void parkVehicle(ParkingSpot spot, Vehicle vehicle);
    void unparkVehicle(ParkingSpot spot);
}

class NoVehicleState implements ParkingSpotState {
    static final NoVehicleState INSTANCE = new NoVehicleState();

    @Override
    public void parkVehicle(ParkingSpot spot, Vehicle vehicle) {
        spot.setVehicle(vehicle);
        spot.setState(HasVehicleState.INSTANCE);
    }

    @Override
    public void unparkVehicle(ParkingSpot spot) {
        throw new IllegalStateException("Spot " + spot.getId() + " has no vehicle to unpark");
    }
}

class HasVehicleState implements ParkingSpotState {
    static final HasVehicleState INSTANCE = new HasVehicleState();

    @Override
    public void parkVehicle(ParkingSpot spot, Vehicle vehicle) {
        throw new IllegalStateException("Spot " + spot.getId() + " is already occupied");
    }

    @Override
    public void unparkVehicle(ParkingSpot spot) {
        spot.setVehicle(null);
        spot.setState(NoVehicleState.INSTANCE);
    }
}

class ParkingSpot {
    private final String id;
    private final SpotType type;
    private Vehicle vehicle;
    private ParkingSpotState currentState = NoVehicleState.INSTANCE;

    ParkingSpot(String id, SpotType type) {
        this.id = id;
        this.type = type;
    }

    boolean isOccupied() {
        return currentState == HasVehicleState.INSTANCE;
    }

    boolean canFit(VehicleType vehicleType) {
        switch (type) {
            case BIKE: return vehicleType == VehicleType.BIKE;
            case COMPACT: return vehicleType == VehicleType.CAR;
            case LARGE: return vehicleType == VehicleType.CAR || vehicleType == VehicleType.TRUCK;
            default: return false;
        }
    }

    void parkVehicle(Vehicle vehicle) {
        currentState.parkVehicle(this, vehicle);
    }

    void unparkVehicle() {
        currentState.unparkVehicle(this);
    }

    String getId() {
        return id;
    }

    void setVehicle(Vehicle vehicle) {
        this.vehicle = vehicle;
    }

    void setState(ParkingSpotState state) {
        this.currentState = state;
    }
}

class ParkingFloor {
    final int floorNumber;
    final List<ParkingSpot> spots;

    ParkingFloor(int floorNumber, List<ParkingSpot> spots) {
        this.floorNumber = floorNumber;
        this.spots = spots;
    }
}

class Ticket {
    final String id;
    final String licenseNumber;
    final VehicleType vehicleType;
    final LocalDateTime entryTime;
    final ParkingSpot spotAssigned;
    TicketStatus status = TicketStatus.ACTIVE;
    LocalDateTime exitTime;
    double amount;

    Ticket(String id, String licenseNumber, VehicleType vehicleType, LocalDateTime entryTime, ParkingSpot spotAssigned) {
        this.id = id;
        this.licenseNumber = licenseNumber;
        this.vehicleType = vehicleType;
        this.entryTime = entryTime;
        this.spotAssigned = spotAssigned;
    }
}

interface PriceCalculationStrategy {
    double calculatePrice(Ticket ticket);
}

class HourlyRateStrategy implements PriceCalculationStrategy {
    private static final Map<VehicleType, Double> RATES = Map.of(
            VehicleType.BIKE, 10.0,
            VehicleType.CAR, 20.0,
            VehicleType.TRUCK, 30.0
    );

    @Override
    public double calculatePrice(Ticket ticket) {
        long minutes = ChronoUnit.MINUTES.between(ticket.entryTime, ticket.exitTime);
        long hours = Math.max(1, (long) Math.ceil(minutes / 60.0));
        return hours * RATES.get(ticket.vehicleType);
    }
}

interface PaymentStrategy {
    boolean pay(double amount);
}

class CashPayment implements PaymentStrategy {
    @Override
    public boolean pay(double amount) {
        System.out.printf("  Paid $%.2f in cash%n", amount);
        return true;
    }
}

class CardPayment implements PaymentStrategy {
    @Override
    public boolean pay(double amount) {
        System.out.printf("  Paid $%.2f by card%n", amount);
        return true;
    }
}

class NoSpotAvailableException extends RuntimeException {
    NoSpotAvailableException(String message) {
        super(message);
    }
}

class EntryGate {
    final String id;
    private final ParkingLotManager manager;

    EntryGate(String id, ParkingLotManager manager) {
        this.id = id;
        this.manager = manager;
    }

    Ticket registerVehicle(Vehicle vehicle) {
        return registerVehicle(vehicle, LocalDateTime.now());
    }

    Ticket registerVehicle(Vehicle vehicle, LocalDateTime entryTime) {
        // 1. finding the parkingSpot
        ParkingSpot spot = manager.findAvailableSpot(vehicle.type)
                .orElseThrow(() -> new NoSpotAvailableException("No available spot for vehicle type " + vehicle.type));
        // 2. park the vehicle
        spot.parkVehicle(vehicle);
        // 3. return the ticket to the user
        Ticket ticket = manager.createTicket(vehicle, spot, entryTime);
        System.out.println("[" + id + "] Issued " + ticket.id + " for " + vehicle.licenseNumber + " -> spot " + spot.getId());
        return ticket;
    }
}

class ExitGate {
    final String id;
    private final PriceCalculationStrategy priceStrategy;
    private final PaymentStrategy paymentStrategy;

    ExitGate(String id, PriceCalculationStrategy priceStrategy, PaymentStrategy paymentStrategy) {
        this.id = id;
        this.priceStrategy = priceStrategy;
        this.paymentStrategy = paymentStrategy;
    }

    boolean unregisterVehicle(Ticket ticket) {
        return unregisterVehicle(ticket, LocalDateTime.now());
    }

    boolean unregisterVehicle(Ticket ticket, LocalDateTime exitTime) {
        if (ticket.status != TicketStatus.ACTIVE) {
            throw new IllegalStateException("Ticket " + ticket.id + " is not active");
        }
        System.out.println("[" + id + "] Closing " + ticket.id + " for " + ticket.licenseNumber);

        ticket.exitTime = exitTime;
        double amount = priceStrategy.calculatePrice(ticket);
        // 1. unpark the vehicle
        ticket.spotAssigned.unparkVehicle();
        // 2. generate the payment
        boolean paid = paymentStrategy.pay(amount);
        ticket.amount = amount;
        ticket.status = paid ? TicketStatus.CLOSED : TicketStatus.ACTIVE;
        System.out.println("  Freed spot " + ticket.spotAssigned.getId());
        return paid;
    }
}

// Keep it singleton
class ParkingLotManager {
    private static ParkingLotManager instance;

    private final List<ParkingFloor> floors = new ArrayList<>();
    private final Map<String, EntryGate> entryGates = new HashMap<>();
    private final Map<String, ExitGate> exitGates = new HashMap<>();
    private final AtomicInteger ticketSequence = new AtomicInteger(1);

    private ParkingLotManager() {
    }

    static synchronized ParkingLotManager getInstance() {
        if (instance == null) {
            instance = new ParkingLotManager();
        }
        return instance;
    }

    void addFloor(ParkingFloor floor) {
        floors.add(floor);
    }

    void addEntryGate(EntryGate gate) {
        entryGates.put(gate.id, gate);
    }

    void addExitGate(ExitGate gate) {
        exitGates.put(gate.id, gate);
    }

    Optional<ParkingSpot> findAvailableSpot(VehicleType vehicleType) {
        for (ParkingFloor floor : floors) {
            for (ParkingSpot spot : floor.spots) {
                if (!spot.isOccupied() && spot.canFit(vehicleType)) {
                    return Optional.of(spot);
                }
            }
        }
        return Optional.empty();
    }

    Ticket createTicket(Vehicle vehicle, ParkingSpot spot, LocalDateTime entryTime) {
        String ticketId = "T" + ticketSequence.getAndIncrement();
        return new Ticket(ticketId, vehicle.licenseNumber, vehicle.type, entryTime, spot);
    }

    Ticket registerVehicle(Vehicle vehicle, String entryGateId) {
        return registerVehicle(vehicle, entryGateId, LocalDateTime.now());
    }

    Ticket registerVehicle(Vehicle vehicle, String entryGateId, LocalDateTime entryTime) {
        EntryGate entrygate = entryGates.get(entryGateId);
        if (entrygate == null) {
            throw new IllegalArgumentException("Unknown entry gate " + entryGateId);
        }
        return entrygate.registerVehicle(vehicle, entryTime);
    }

    boolean unregisterVehicle(Ticket ticket, String exitGateId) {
        ExitGate exitGate = exitGates.get(exitGateId);
        if (exitGate == null) {
            throw new IllegalArgumentException("Unknown exit gate " + exitGateId);
        }
        return exitGate.unregisterVehicle(ticket);
    }
}

public class ParkingLot_demo {
    public static void main(String[] args) {
        ParkingLotManager manager = ParkingLotManager.getInstance();

        manager.addFloor(new ParkingFloor(1, new ArrayList<>(List.of(
                new ParkingSpot("F1-B1", SpotType.BIKE),
                new ParkingSpot("F1-C1", SpotType.COMPACT),
                new ParkingSpot("F1-L1", SpotType.LARGE)
        ))));

        EntryGate entryGate = new EntryGate("Entry-1", manager);
        manager.addEntryGate(entryGate);

        manager.addExitGate(new ExitGate("Exit-1", new HourlyRateStrategy(), new CashPayment()));
        manager.addExitGate(new ExitGate("Exit-2", new HourlyRateStrategy(), new CardPayment()));

        LocalDateTime now = LocalDateTime.now();

        Vehicle bike = new Bike("KA-01-1234");
        Vehicle car = new Car("KA-02-5678");
        Vehicle truck = new Truck("KA-03-9999");

        Ticket bikeTicket = manager.registerVehicle(bike, "Entry-1", now.minusMinutes(30));
        Ticket carTicket = manager.registerVehicle(car, "Entry-1", now.minusMinutes(135));
        Ticket truckTicket = manager.registerVehicle(truck, "Entry-1", now.minusHours(1));

        System.out.println("\nAll compact/large spots are now full -- another car must wait:");
        Vehicle secondCar = new Car("KA-04-1111");
        try {
            manager.registerVehicle(secondCar, "Entry-1", now);
        } catch (NoSpotAvailableException e) {
            System.out.println("  " + e.getMessage());
        }

        System.out.println("\nProcessing exits:");
        manager.unregisterVehicle(bikeTicket, "Exit-1");
        manager.unregisterVehicle(carTicket, "Exit-2");
        manager.unregisterVehicle(truckTicket, "Exit-1");

        System.out.println("\nSpot now free -- the waiting car can park:");
        manager.registerVehicle(secondCar, "Entry-1", now);
    }
}
