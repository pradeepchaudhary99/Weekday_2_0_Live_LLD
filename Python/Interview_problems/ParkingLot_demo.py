"""
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
"""

import math
import threading
from abc import ABC, abstractmethod
from datetime import datetime, timedelta
from enum import Enum, auto
from typing import Dict, List, Optional


class VehicleType(Enum):
    BIKE = auto()
    CAR = auto()
    TRUCK = auto()


class SpotType(Enum):
    BIKE = auto()
    COMPACT = auto()
    LARGE = auto()


class TicketStatus(Enum):
    ACTIVE = auto()
    PAID = auto()
    CLOSED = auto()


# A spot can host a vehicle type if the spot's category is large enough.
_SPOT_FITS: Dict[SpotType, List[VehicleType]] = {
    SpotType.BIKE: [VehicleType.BIKE],
    SpotType.COMPACT: [VehicleType.CAR],
    SpotType.LARGE: [VehicleType.CAR, VehicleType.TRUCK],
}


class Vehicle:
    def __init__(self, license_number: str, vehicle_type: VehicleType):
        self.license_number = license_number
        self.type = vehicle_type


class ParkingSpot:
    def __init__(self, id: str, floor_number: int, spot_type: SpotType):
        self.id = id
        self.floor_number = floor_number
        self.type = spot_type
        self.vehicle: Optional[Vehicle] = None

    def is_occupied(self) -> bool:
        return self.vehicle is not None

    def can_fit(self, vehicle_type: VehicleType) -> bool:
        return vehicle_type in _SPOT_FITS[self.type]

    def park(self, vehicle: Vehicle) -> None:
        self.vehicle = vehicle

    def vacate(self) -> None:
        self.vehicle = None


class ParkingFloor:
    def __init__(self, floor_number: int, spots: List[ParkingSpot]):
        self.floor_number = floor_number
        self.spots = spots


class ParkingSpotAllocationStrategy(ABC):
    @abstractmethod
    def find_spot(self, floors: List[ParkingFloor], vehicle_type: VehicleType) -> Optional[ParkingSpot]:
        raise NotImplementedError


class NearestAvailableSpotStrategy(ParkingSpotAllocationStrategy):
    def find_spot(self, floors: List[ParkingFloor], vehicle_type: VehicleType) -> Optional[ParkingSpot]:
        for floor in floors:
            for spot in floor.spots:
                if not spot.is_occupied() and spot.can_fit(vehicle_type):
                    return spot
        return None


class Ticket:
    def __init__(self, id: str, vehicle: Vehicle, spot: ParkingSpot, entry_time: datetime):
        self.id = id
        self.vehicle = vehicle
        self.spot = spot
        self.entry_time = entry_time
        self.exit_time: Optional[datetime] = None
        self.amount: float = 0.0
        self.status = TicketStatus.ACTIVE


class PriceCalculationStrategy(ABC):
    @abstractmethod
    def calculate_fee(self, ticket: Ticket) -> float:
        raise NotImplementedError


class HourlyRateStrategy(PriceCalculationStrategy):
    _RATES: Dict[VehicleType, float] = {
        VehicleType.BIKE: 10.0,
        VehicleType.CAR: 20.0,
        VehicleType.TRUCK: 30.0,
    }

    def calculate_fee(self, ticket: Ticket) -> float:
        duration = (ticket.exit_time - ticket.entry_time).total_seconds() / 3600.0
        hours = max(1, math.ceil(duration))
        return hours * self._RATES[ticket.vehicle.type]


class PaymentStrategy(ABC):
    @abstractmethod
    def pay(self, amount: float) -> bool:
        raise NotImplementedError


class CashPayment(PaymentStrategy):
    def pay(self, amount: float) -> bool:
        print(f"  Paid ${amount:.2f} in cash")
        return True


class CardPayment(PaymentStrategy):
    def pay(self, amount: float) -> bool:
        print(f"  Paid ${amount:.2f} by card")
        return True


class ParkingLotFullError(RuntimeError):
    pass


class ParkingLot:
    def __init__(self, floors: List[ParkingFloor], allocation_strategy: ParkingSpotAllocationStrategy,
                 price_strategy: PriceCalculationStrategy):
        self.floors = floors
        self.allocation_strategy = allocation_strategy
        self.price_strategy = price_strategy
        self._lock = threading.RLock()
        self._next_ticket_id = 1

    def _generate_ticket_id(self) -> str:
        with self._lock:
            ticket_id = f"T{self._next_ticket_id}"
            self._next_ticket_id += 1
            return ticket_id

    def park_vehicle(self, vehicle: Vehicle, entry_time: Optional[datetime] = None) -> Ticket:
        with self._lock:
            spot = self.allocation_strategy.find_spot(self.floors, vehicle.type)
            if spot is None:
                raise ParkingLotFullError(f"No available spot for vehicle type {vehicle.type.name}")
            spot.park(vehicle)
            ticket = Ticket(self._generate_ticket_id(), vehicle, spot, entry_time or datetime.now())
            return ticket

    def unpark_vehicle(self, ticket: Ticket, payment_strategy: PaymentStrategy,
                        exit_time: Optional[datetime] = None) -> float:
        if ticket.status != TicketStatus.ACTIVE:
            raise ValueError(f"Ticket {ticket.id} is not active")
        ticket.exit_time = exit_time or datetime.now()
        ticket.amount = self.price_strategy.calculate_fee(ticket)
        payment_strategy.pay(ticket.amount)
        ticket.status = TicketStatus.PAID
        ticket.spot.vacate()
        ticket.status = TicketStatus.CLOSED
        return ticket.amount


class EntryGate:
    def __init__(self, id: str, parking_lot: ParkingLot):
        self.id = id
        self.parking_lot = parking_lot

    def issue_ticket(self, vehicle: Vehicle, entry_time: Optional[datetime] = None) -> Ticket:
        ticket = self.parking_lot.park_vehicle(vehicle, entry_time)
        print(f"[{self.id}] Issued {ticket.id} for {vehicle.license_number} -> spot {ticket.spot.id}")
        return ticket


class ExitGate:
    def __init__(self, id: str, parking_lot: ParkingLot):
        self.id = id
        self.parking_lot = parking_lot

    def process_exit(self, ticket: Ticket, payment_strategy: PaymentStrategy,
                      exit_time: Optional[datetime] = None) -> float:
        print(f"[{self.id}] Closing {ticket.id} for {ticket.vehicle.license_number}")
        amount = self.parking_lot.unpark_vehicle(ticket, payment_strategy, exit_time)
        print(f"  Freed spot {ticket.spot.id}")
        return amount


def _build_floor(floor_number: int, bike_spots: int, compact_spots: int, large_spots: int) -> ParkingFloor:
    spots: List[ParkingSpot] = []
    for i in range(bike_spots):
        spots.append(ParkingSpot(f"F{floor_number}-B{i + 1}", floor_number, SpotType.BIKE))
    for i in range(compact_spots):
        spots.append(ParkingSpot(f"F{floor_number}-C{i + 1}", floor_number, SpotType.COMPACT))
    for i in range(large_spots):
        spots.append(ParkingSpot(f"F{floor_number}-L{i + 1}", floor_number, SpotType.LARGE))
    return ParkingFloor(floor_number, spots)


def main() -> None:
    floors = [_build_floor(1, bike_spots=1, compact_spots=1, large_spots=1)]
    lot = ParkingLot(floors, NearestAvailableSpotStrategy(), HourlyRateStrategy())
    entry_gate = EntryGate("Entry-1", lot)
    exit_gate = ExitGate("Exit-1", lot)

    now = datetime.now()

    bike = Vehicle("KA-01-1234", VehicleType.BIKE)
    car = Vehicle("KA-02-5678", VehicleType.CAR)
    truck = Vehicle("KA-03-9999", VehicleType.TRUCK)

    bike_ticket = entry_gate.issue_ticket(bike, entry_time=now - timedelta(minutes=30))
    car_ticket = entry_gate.issue_ticket(car, entry_time=now - timedelta(hours=2, minutes=15))
    truck_ticket = entry_gate.issue_ticket(truck, entry_time=now - timedelta(hours=1))

    print("\nAll compact/large spots are now full -- another car must wait:")
    second_car = Vehicle("KA-04-1111", VehicleType.CAR)
    try:
        entry_gate.issue_ticket(second_car, entry_time=now)
    except ParkingLotFullError as e:
        print(f"  {e}")

    print("\nProcessing exits:")
    exit_gate.process_exit(bike_ticket, CashPayment(), exit_time=now)
    exit_gate.process_exit(car_ticket, CardPayment(), exit_time=now)
    exit_gate.process_exit(truck_ticket, CashPayment(), exit_time=now)

    print("\nSpot now free -- the waiting car can park:")
    entry_gate.issue_ticket(second_car, entry_time=now)


if __name__ == "__main__":
    main()
