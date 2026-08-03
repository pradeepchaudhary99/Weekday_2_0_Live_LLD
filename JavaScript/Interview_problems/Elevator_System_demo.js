/*
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

*/

// Core Entities
// Elevator
// Request
// SchedulingStrategy
// NearestElevatorStrategy
// ElevatorSystem // ElevatorController
//

const Direction = Object.freeze({ UP: "UP", DOWN: "DOWN", IDLE: "IDLE" });

const ElevatorStates = Object.freeze({
    MOVING: "MOVING",
    STOPPED: "STOPPED",
    IDLE: "IDLE",
    MAINTAINENCE: "MAINTAINENCE",
});

const DoorState = Object.freeze({ OPEN: "OPEN", CLOSED: "CLOSED" });

class Request {
    constructor() {
        this.floor = 0;
        this.direction = null;
    }
}

class SchedulingStrategy {
    getElevator(elevators, request) {
        throw new Error("Method 'getElevator()' must be implemented.");
    }
}

class NearestElevatorStrategy extends SchedulingStrategy {
    getElevator(elevators, request) {
    }
}

class SortedFloors {
    // Mirrors java.util.TreeSet ordering (ascending or descending)
    constructor(descending = false) {
        this.items = [];
        this.descending = descending;
    }

    add(value) {
        let index = this.items.findIndex((item) =>
            this.descending ? item < value : item > value
        );
        if (index === -1) index = this.items.length;
        this.items.splice(index, 0, value);
    }

    isEmpty() {
        return this.items.length === 0;
    }

    first() {
        return this.items[0];
    }

    pollFirst() {
        return this.items.shift();
    }
}

class Elevator {
    constructor() {
        this.id = null;
        this.state = ElevatorStates.IDLE;
        this.doorState = DoorState.CLOSED;
        this.currentFloor = 0;
        this.upStops = new SortedFloors(false);   // Increasing Order
        this.downStops = new SortedFloors(true);  // Decreasing Order
        this.running = false;
        this.direction = Direction.IDLE;
    }

    startElevator() {
        this.running = true;
    }

    stopElevator() {
        this.running = false;
    }

    addRequest(request) {
        if (request.floor > this.currentFloor) {
            this.upStops.add(request.floor);
        } else if (request.floor < this.currentFloor) {
            this.downStops.add(request.floor);
        }
    }

    async run() {
        while (this.running) {
            this.step();
            await new Promise((resolve) => setTimeout(resolve, 150)); // simulate
        }
    }

    step() {
        const dir = this.direction;
        if (dir === Direction.UP) {
            if (this.upStops.isEmpty()) {
                this.direction = this.downStops.isEmpty() ? Direction.IDLE : Direction.DOWN;
                this.state = this.downStops.isEmpty() ? ElevatorStates.IDLE : ElevatorStates.MOVING;
                return;
            }
            if (this.currentFloor === this.upStops.first()) {
                this.upStops.pollFirst();
                // STOPPING the Elevator
                // 3-4 lines of logic for opening/closing the doors
            }
            this.currentFloor++;
        } else if (dir === Direction.DOWN) {
            if (this.downStops.isEmpty()) {
                this.direction = this.upStops.isEmpty() ? Direction.IDLE : Direction.UP;
                this.state = this.upStops.isEmpty() ? ElevatorStates.IDLE : ElevatorStates.MOVING;
                return;
            }
            if (this.currentFloor === this.downStops.first()) {
                this.downStops.pollFirst();
                // STOPPING the Elevator
                // 3-4 lines of logic for opening/closing the doors
            }
            this.currentFloor--;
        } else {
            this.state = ElevatorStates.IDLE;
        }
    }
}

class ElevatorSystem {
    constructor() {
        this.elevators = [];
        this.threads = [];
        this.strategy = null;

        this.numberOfElevators = 5;
        for (let i = 1; i <= 5; i++) {
            const elevator = new Elevator();
            this.elevators.push(elevator);
            this.threads.push(elevator);
        }
    }

    addRequest(request) {
        const elevator = this.strategy.getElevator(this.elevators, request);
        elevator.addRequest(request);
    }
}

class ElevatorSystemDemo {
}

module.exports = {
    Direction,
    ElevatorStates,
    DoorState,
    Request,
    SchedulingStrategy,
    NearestElevatorStrategy,
    Elevator,
    ElevatorSystem,
    ElevatorSystemDemo,
};
