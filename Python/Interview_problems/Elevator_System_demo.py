"""
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
       subclass, not a rewrite of ElevatorSystem.
    2. Thread-safety: each elevator runs on its own thread and mutates its
       own stop-queues/state; those fields are guarded by a per-elevator
       lock since requests can arrive concurrently from many callers.
    3. No duplicate requests: stop floors are kept in a sorted set, so
       requesting the same floor twice is a no-op, not a double visit.
    4. Fault isolation: one elevator's thread never touches another
       elevator's state, so a bug/slowdown in one cabin can't corrupt or
       block the rest of the fleet.

--------------------------------------------------------------------------
Design
--------------------------------------------------------------------------
Strategy pattern for assignment:
    ElevatorSystem never hardcodes "closest elevator wins" -- it asks a
    SchedulingStrategy. NearestElevatorStrategy is one implementation;
    swapping in e.g. a "least busy" or SCAN-based strategy means adding a
    class, not editing ElevatorSystem.

Sorted stop-queues drive the movement loop:
    Each Elevator keeps two sorted-float sets: up_stops (visited ascending,
    so the smallest is always the next stop while moving up) and
    down_stops (visited descending, so the largest is always the next stop
    while moving down). One step() call either advances one floor or
    services a stop; the background thread just calls step() on a fixed
    cadence -- this is what turns "the elevator has some destinations"
    into actual motion without busy-polling a shared queue.

Core Entities:
    Direction             -- UP, DOWN, IDLE
    ElevatorState          -- MOVING, STOPPED, IDLE, MAINTENANCE
    DoorState              -- OPEN, CLOSED
    Request                -- floor (+ direction, for external hall calls)
    SchedulingStrategy      -- assignment interface
    NearestElevatorStrategy
    Elevator               -- one cabin's state machine + movement thread
    ElevatorSystem          -- owns the fleet, routes requests via the strategy
================================================================================
"""

import bisect
import threading
import time
from abc import ABC, abstractmethod
from enum import Enum, auto
from typing import List, Optional


class SortedFloorSet:
    """Minimal ascending sorted-set of floor numbers, stdlib-only (no
    external dependency): O(log n) membership/insert position via bisect,
    O(n) insert/remove -- fine at the "handful of pending stops" scale a
    single elevator ever queues up."""

    def __init__(self) -> None:
        self._items: List[int] = []

    def add(self, value: int) -> None:
        i = bisect.bisect_left(self._items, value)
        if i == len(self._items) or self._items[i] != value:
            self._items.insert(i, value)

    def __bool__(self) -> bool:
        return bool(self._items)

    def __getitem__(self, index: int) -> int:
        return self._items[index]

    def pop(self, index: int) -> int:
        return self._items.pop(index)


class Direction(Enum):
    UP = auto()
    DOWN = auto()
    IDLE = auto()


class ElevatorState(Enum):
    MOVING = auto()
    STOPPED = auto()
    IDLE = auto()
    MAINTENANCE = auto()


class DoorState(Enum):
    OPEN = auto()
    CLOSED = auto()


class Request:
    def __init__(self, floor: int, direction: Optional[Direction] = None):
        self.floor = floor
        self.direction = direction  # None for an internal cabin request


class SchedulingStrategy(ABC):
    @abstractmethod
    def get_elevator(self, elevators: List["Elevator"], request: Request) -> "Elevator":
        raise NotImplementedError


class NearestElevatorStrategy(SchedulingStrategy):
    """Picks whichever elevator is numerically closest to the requested
    floor, with a preference for elevators that are currently idle (an
    idle elevator can retarget immediately; a moving one should ideally
    finish its current direction first, which a fancier SCAN-style
    strategy would model -- left as an extension point)."""

    def get_elevator(self, elevators: List["Elevator"], request: Request) -> "Elevator":
        best = None
        best_score = float("inf")
        for elevator in elevators:
            floor = elevator.get_current_floor()
            idle = elevator.is_idle()
            distance = abs(floor - request.floor)
            # Idle elevators are scored as if they were half as far away,
            # so a busy elevator only wins when it is clearly closer.
            score = distance if idle else distance * 2
            if score < best_score:
                best_score = score
                best = elevator
        return best


class Elevator:
    def __init__(self, elevator_id: int, starting_floor: int = 0):
        self.id = elevator_id
        self._lock = threading.Lock()
        self.state = ElevatorState.IDLE
        self.door_state = DoorState.CLOSED
        self._current_floor = starting_floor
        self.direction = Direction.IDLE
        self._up_stops = SortedFloorSet()    # visited ascending
        self._down_stops = SortedFloorSet()  # visited descending (via [-1])
        self.running = False

    def start_elevator(self) -> None:
        self.running = True

    def stop_elevator(self) -> None:
        self.running = False

    def get_current_floor(self) -> int:
        with self._lock:
            return self._current_floor

    def is_idle(self) -> bool:
        with self._lock:
            return self.direction == Direction.IDLE and not self._up_stops and not self._down_stops

    def add_request(self, floor: int) -> None:
        with self._lock:
            if floor > self._current_floor:
                self._up_stops.add(floor)
                if self.direction == Direction.IDLE:
                    self.direction = Direction.UP
            elif floor < self._current_floor:
                self._down_stops.add(floor)
                if self.direction == Direction.IDLE:
                    self.direction = Direction.DOWN
            # floor == current_floor: already there, nothing to queue.

    def run(self) -> None:
        while self.running:
            self.step()
            time.sleep(0.05)  # simulate the time to travel one floor

    def step(self) -> None:
        """Advances the elevator by exactly one unit of work: either it
        opens its doors for a stop it has just reached, or it moves one
        floor toward the next stop, or -- with nothing left to do -- it
        goes idle."""
        with self._lock:
            if self.direction == Direction.UP:
                if not self._up_stops:
                    self.direction = Direction.IDLE if not self._down_stops else Direction.DOWN
                    self.state = ElevatorState.IDLE if self.direction == Direction.IDLE else ElevatorState.MOVING
                    return
                if self._current_floor == self._up_stops[0]:
                    self._up_stops.pop(0)
                    self._open_and_close_doors()
                    return
                self.state = ElevatorState.MOVING
                self._current_floor += 1
            elif self.direction == Direction.DOWN:
                if not self._down_stops:
                    self.direction = Direction.IDLE if not self._up_stops else Direction.UP
                    self.state = ElevatorState.IDLE if self.direction == Direction.IDLE else ElevatorState.MOVING
                    return
                if self._current_floor == self._down_stops[-1]:
                    self._down_stops.pop(-1)
                    self._open_and_close_doors()
                    return
                self.state = ElevatorState.MOVING
                self._current_floor -= 1
            else:
                self.state = ElevatorState.IDLE

    def _open_and_close_doors(self) -> None:
        """Called with self._lock already held."""
        self.state = ElevatorState.STOPPED
        self.door_state = DoorState.OPEN
        print(f"Elevator {self.id} arrived at floor {self._current_floor}, doors OPEN")
        self.door_state = DoorState.CLOSED
        print(f"Elevator {self.id} doors CLOSED at floor {self._current_floor}")


class ElevatorSystem:
    def __init__(self, number_of_elevators: int, strategy: SchedulingStrategy):
        self.strategy = strategy
        self.elevators: List[Elevator] = []
        self._threads: List[threading.Thread] = []
        for i in range(1, number_of_elevators + 1):
            elevator = Elevator(i, starting_floor=0)
            elevator.start_elevator()
            thread = threading.Thread(target=elevator.run, name=f"elevator-{i}", daemon=True)
            self.elevators.append(elevator)
            self._threads.append(thread)
            thread.start()

    def add_request(self, request: Request) -> None:
        elevator = self.strategy.get_elevator(self.elevators, request)
        elevator.add_request(request.floor)
        print(f"Assigned request for floor {request.floor} to elevator {elevator.id}")

    def await_all_idle(self, timeout_s: float) -> bool:
        """No busy-polling on the caller's side either: sleep between
        checks instead of spinning, and give up after timeout_s so a bug
        in one elevator can't hang the demo forever."""
        deadline = time.time() + timeout_s
        while time.time() < deadline:
            if all(elevator.is_idle() for elevator in self.elevators):
                return True
            time.sleep(0.02)
        return False

    def print_fleet_status(self) -> None:
        for elevator in self.elevators:
            print(f"  Elevator {elevator.id} resting at floor {elevator.get_current_floor()}")

    def shutdown(self) -> None:
        for elevator in self.elevators:
            elevator.stop_elevator()
        for thread in self._threads:
            thread.join()


def main() -> None:
    system = ElevatorSystem(3, NearestElevatorStrategy())

    system.add_request(Request(5, Direction.UP))
    system.add_request(Request(2, Direction.UP))
    system.add_request(Request(8, Direction.DOWN))
    system.add_request(Request(1, Direction.DOWN))

    finished = system.await_all_idle(5.0)
    print(f"\nAll requests serviced: {finished}")
    print("Final fleet status:")
    system.print_fleet_status()

    system.shutdown()


if __name__ == "__main__":
    main()
