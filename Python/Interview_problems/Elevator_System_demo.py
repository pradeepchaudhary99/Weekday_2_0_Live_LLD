"""
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
"""

# Core Entities
# Elevator
# Request
# SchedulingStrategy
# NearestElevatorStrategy
# ElevatorSystem // ElevatorController
#

import bisect
import threading
import time
import uuid
from abc import ABC, abstractmethod
from enum import Enum, auto
from typing import List, Optional


class Direction(Enum):
    UP = auto()
    DOWN = auto()
    IDLE = auto()


class ElevatorStates(Enum):
    MOVING = auto()
    STOPPED = auto()
    IDLE = auto()
    MAINTAINENCE = auto()


class DoorState(Enum):
    OPEN = auto()
    CLOSED = auto()


class Request:
    def __init__(self) -> None:
        self.floor: int = 0
        self.direction: Optional[Direction] = None


class SchedulingStrategy(ABC):
    @abstractmethod
    def get_elevator(self, elevators: List["Elevator"], request: Request) -> Optional["Elevator"]:
        pass


class NearestElevatorStrategy(SchedulingStrategy):
    def get_elevator(self, elevators: List["Elevator"], request: Request) -> Optional["Elevator"]:
        pass


class _SortedFloors:
    """Mirrors java.util.TreeSet ordering (ascending or descending)."""

    def __init__(self, descending: bool = False) -> None:
        self._items: List[int] = []
        self._descending = descending

    def _key(self, value: int) -> int:
        return -value if self._descending else value

    def add(self, value: int) -> None:
        bisect.insort(self._items, self._key(value))

    def is_empty(self) -> bool:
        return len(self._items) == 0

    def first(self) -> int:
        return self._key(self._items[0])

    def poll_first(self) -> int:
        head = self._items.pop(0)
        return self._key(head)


class Elevator:
    def __init__(self) -> None:
        self.id: uuid.UUID = uuid.uuid4()
        self.state: ElevatorStates = ElevatorStates.IDLE
        self.door_state: DoorState = DoorState.CLOSED
        self.current_floor: int = 0
        self.up_stops = _SortedFloors(descending=False)   # Increasing Order
        self.down_stops = _SortedFloors(descending=True)  # Decreasing Order
        self.running: bool = False
        self.direction: Direction = Direction.IDLE

    def start_elevator(self) -> None:
        self.running = True

    def stop_elevator(self) -> None:
        self.running = False

    def add_request(self, request: Request) -> None:
        if request.floor > self.current_floor:
            self.up_stops.add(request.floor)
        elif request.floor < self.current_floor:
            self.down_stops.add(request.floor)

    def run(self) -> None:
        while self.running:
            self.step()
            time.sleep(0.15)  # simulate

    def step(self) -> None:
        direction = self.direction
        if direction == Direction.UP:
            if self.up_stops.is_empty():
                self.direction = Direction.IDLE if self.down_stops.is_empty() else Direction.DOWN
                self.state = ElevatorStates.IDLE if self.down_stops.is_empty() else ElevatorStates.MOVING
                return
            if self.current_floor == self.up_stops.first():
                self.up_stops.poll_first()
                # STOPPING the Elevator
                # 3-4 lines of logic for opening/closing the doors
            self.current_floor += 1
        elif direction == Direction.DOWN:
            if self.down_stops.is_empty():
                self.direction = Direction.IDLE if self.up_stops.is_empty() else Direction.UP
                self.state = ElevatorStates.IDLE if self.up_stops.is_empty() else ElevatorStates.MOVING
                return
            if self.current_floor == self.down_stops.first():
                self.down_stops.poll_first()
                # STOPPING the Elevator
                # 3-4 lines of logic for opening/closing the doors
            self.current_floor -= 1
        else:
            self.state = ElevatorStates.IDLE


class ElevatorSystem:
    def __init__(self) -> None:
        self.elevators: List[Elevator] = []
        self.threads: List[threading.Thread] = []
        self.strategy: Optional[SchedulingStrategy] = None

        self.number_of_elevators = 5
        for _ in range(1, 6):
            elevator = Elevator()
            thread = threading.Thread(target=elevator.run)
            self.elevators.append(elevator)
            self.threads.append(thread)

    def add_request(self, request: Request) -> None:
        elevator = self.strategy.get_elevator(self.elevators, request)
        elevator.add_request(request)


class ElevatorSystemDemo:
    pass
