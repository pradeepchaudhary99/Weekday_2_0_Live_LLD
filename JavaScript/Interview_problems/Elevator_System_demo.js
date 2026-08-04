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
       subclass, not a rewrite of ElevatorSystem.
    2. Concurrency-safety: Node has no real threads, so there's no data-race
       risk the way there is in Java/C++/Go -- but each elevator still runs
       its own independent async loop (via setTimeout), interleaved by the
       event loop, so state must never be shared across elevators (it
       isn't: every field below lives on the Elevator instance).
    3. No duplicate requests: stop floors are kept in a sorted set, so
       requesting the same floor twice is a no-op, not a double visit.
    4. Fault isolation: one elevator's loop never touches another
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
    Each Elevator keeps two sorted-floor sets: upStops (visited ascending,
    so the smallest is always the next stop while moving up) and
    downStops (visited descending, so the largest is always the next stop
    while moving down). One step() call either advances one floor or
    services a stop; the async run() loop just calls step() on a fixed
    cadence -- this is what turns "the elevator has some destinations"
    into actual motion without busy-polling a shared queue.

Core Entities:
    Direction             -- UP, DOWN, IDLE
    ElevatorState          -- MOVING, STOPPED, IDLE, MAINTENANCE
    DoorState              -- OPEN, CLOSED
    Request                -- floor (+ direction, for external hall calls)
    SchedulingStrategy      -- assignment interface
    NearestElevatorStrategy
    Elevator               -- one cabin's state machine + async movement loop
    ElevatorSystem          -- owns the fleet, routes requests via the strategy
================================================================================
*/

'use strict';

const Direction = Object.freeze({ UP: 'UP', DOWN: 'DOWN', IDLE: 'IDLE' });

const ElevatorState = Object.freeze({
    MOVING: 'MOVING',
    STOPPED: 'STOPPED',
    IDLE: 'IDLE',
    MAINTENANCE: 'MAINTENANCE',
});

const DoorState = Object.freeze({ OPEN: 'OPEN', CLOSED: 'CLOSED' });

class Request {
    constructor(floor, direction = null) {
        this.floor = floor;
        this.direction = direction; // null for an internal cabin request
    }
}

class SchedulingStrategy {
    // eslint-disable-next-line no-unused-vars
    getElevator(elevators, request) {
        throw new Error("Method 'getElevator()' must be implemented.");
    }
}

// Picks whichever elevator is numerically closest to the requested floor,
// with a preference for elevators that are currently idle (an idle
// elevator can retarget immediately; a moving one should ideally finish
// its current direction first, which a fancier SCAN-style strategy would
// model -- left as an extension point).
class NearestElevatorStrategy extends SchedulingStrategy {
    getElevator(elevators, request) {
        let best = null;
        let bestScore = Infinity;
        for (const elevator of elevators) {
            const floor = elevator.currentFloor;
            const idle = elevator.isIdle();
            const distance = Math.abs(floor - request.floor);
            // Idle elevators are scored as if they were half as far away,
            // so a busy elevator only wins when it is clearly closer.
            const score = idle ? distance : distance * 2;
            if (score < bestScore) {
                bestScore = score;
                best = elevator;
            }
        }
        return best;
    }
}

class SortedFloors {
    // Minimal ascending sorted-set of floor numbers: O(n) insert/remove,
    // which is fine at the "handful of pending stops" scale a single
    // elevator ever queues up.
    constructor() {
        this.items = [];
    }

    add(value) {
        if (this.items.includes(value)) return; // no duplicate stops
        let index = this.items.findIndex((item) => item > value);
        if (index === -1) index = this.items.length;
        this.items.splice(index, 0, value);
    }

    isEmpty() {
        return this.items.length === 0;
    }

    first() {
        return this.items[0];
    }

    last() {
        return this.items[this.items.length - 1];
    }

    pollFirst() {
        return this.items.shift();
    }

    pollLast() {
        return this.items.pop();
    }
}

class Elevator {
    constructor(id, startingFloor = 0) {
        this.id = id;
        this.state = ElevatorState.IDLE;
        this.doorState = DoorState.CLOSED;
        this.currentFloor = startingFloor;
        this.direction = Direction.IDLE;
        this.upStops = new SortedFloors();   // visited ascending
        this.downStops = new SortedFloors(); // visited descending (via last/pollLast)
        this.running = false;
    }

    startElevator() {
        this.running = true;
    }

    stopElevator() {
        this.running = false;
    }

    isIdle() {
        return this.direction === Direction.IDLE && this.upStops.isEmpty() && this.downStops.isEmpty();
    }

    addRequest(floor) {
        if (floor > this.currentFloor) {
            this.upStops.add(floor);
            if (this.direction === Direction.IDLE) {
                this.direction = Direction.UP;
            }
        } else if (floor < this.currentFloor) {
            this.downStops.add(floor);
            if (this.direction === Direction.IDLE) {
                this.direction = Direction.DOWN;
            }
        }
        // floor === this.currentFloor: already there, nothing to queue.
    }

    async run() {
        while (this.running) {
            this.step();
            await sleep(50); // simulate the time to travel one floor
        }
    }

    // Advances the elevator by exactly one unit of work: either it opens
    // its doors for a stop it has just reached, or it moves one floor
    // toward the next stop, or -- with nothing left to do -- it goes idle.
    step() {
        if (this.direction === Direction.UP) {
            if (this.upStops.isEmpty()) {
                this.direction = this.downStops.isEmpty() ? Direction.IDLE : Direction.DOWN;
                this.state = this.direction === Direction.IDLE ? ElevatorState.IDLE : ElevatorState.MOVING;
                return;
            }
            if (this.currentFloor === this.upStops.first()) {
                this.upStops.pollFirst();
                this._openAndCloseDoors();
                return;
            }
            this.state = ElevatorState.MOVING;
            this.currentFloor++;
        } else if (this.direction === Direction.DOWN) {
            if (this.downStops.isEmpty()) {
                this.direction = this.upStops.isEmpty() ? Direction.IDLE : Direction.UP;
                this.state = this.direction === Direction.IDLE ? ElevatorState.IDLE : ElevatorState.MOVING;
                return;
            }
            if (this.currentFloor === this.downStops.last()) {
                this.downStops.pollLast();
                this._openAndCloseDoors();
                return;
            }
            this.state = ElevatorState.MOVING;
            this.currentFloor--;
        } else {
            this.state = ElevatorState.IDLE;
        }
    }

    _openAndCloseDoors() {
        this.state = ElevatorState.STOPPED;
        this.doorState = DoorState.OPEN;
        console.log(`Elevator ${this.id} arrived at floor ${this.currentFloor}, doors OPEN`);
        this.doorState = DoorState.CLOSED;
        console.log(`Elevator ${this.id} doors CLOSED at floor ${this.currentFloor}`);
    }
}

function sleep(ms) {
    return new Promise((resolve) => setTimeout(resolve, ms));
}

class ElevatorSystem {
    constructor(numberOfElevators, strategy) {
        this.strategy = strategy;
        this.elevators = [];
        this._runPromises = [];
        for (let i = 1; i <= numberOfElevators; i++) {
            const elevator = new Elevator(i, 0);
            elevator.startElevator();
            this.elevators.push(elevator);
            this._runPromises.push(elevator.run());
        }
    }

    addRequest(request) {
        const elevator = this.strategy.getElevator(this.elevators, request);
        elevator.addRequest(request.floor);
        console.log(`Assigned request for floor ${request.floor} to elevator ${elevator.id}`);
    }

    // No busy-polling on the caller's side either: await a short sleep
    // between checks instead of spinning, and give up after timeoutMs so a
    // bug in one elevator can't hang the demo forever.
    async awaitAllIdle(timeoutMs) {
        const deadline = Date.now() + timeoutMs;
        while (Date.now() < deadline) {
            if (this.elevators.every((elevator) => elevator.isIdle())) {
                return true;
            }
            await sleep(20);
        }
        return false;
    }

    printFleetStatus() {
        for (const elevator of this.elevators) {
            console.log(`  Elevator ${elevator.id} resting at floor ${elevator.currentFloor}`);
        }
    }

    async shutdown() {
        for (const elevator of this.elevators) {
            elevator.stopElevator();
        }
        await Promise.all(this._runPromises);
    }
}

async function main() {
    const system = new ElevatorSystem(3, new NearestElevatorStrategy());

    system.addRequest(new Request(5, Direction.UP));
    system.addRequest(new Request(2, Direction.UP));
    system.addRequest(new Request(8, Direction.DOWN));
    system.addRequest(new Request(1, Direction.DOWN));

    const finished = await system.awaitAllIdle(5000);
    console.log(`\nAll requests serviced: ${finished}`);
    console.log('Final fleet status:');
    system.printFleetStatus();

    await system.shutdown();
}

if (require.main === module) {
    main();
}

module.exports = {
    Direction,
    ElevatorState,
    DoorState,
    Request,
    SchedulingStrategy,
    NearestElevatorStrategy,
    Elevator,
    ElevatorSystem,
};
