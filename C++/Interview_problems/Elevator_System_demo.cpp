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

#include <chrono>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

enum class Direction { UP, DOWN, IDLE };

enum class ElevatorStates { MOVING, STOPPED, IDLE, MAINTAINENCE };

enum class DoorState { OPEN, CLOSED };

struct Request {
    int floor = 0;
    Direction direction = Direction::IDLE;
};

class Elevator;

struct SchedulingStrategy {
    virtual Elevator* getElevator(std::vector<std::unique_ptr<Elevator>>& elevators, const Request& request) = 0;
    virtual ~SchedulingStrategy() = default;
};

class NearestElevatorStrategy : public SchedulingStrategy {
public:
    Elevator* getElevator(std::vector<std::unique_ptr<Elevator>>& elevators, const Request& request) override {
        return nullptr;
    }
};

class Elevator {
public:
    std::string id;
    ElevatorStates state = ElevatorStates::IDLE;
    DoorState doorState = DoorState::CLOSED;
    int currentFloor = 0;
    std::set<int> upStops;                        // Increasing Order
    std::set<int, std::greater<int>> downStops;   // Decreasing Order
    bool running = false;
    Direction direction = Direction::IDLE;

    void startElevator() {
        running = true;
    }

    void stopElevator() {
        running = false;
    }

    void addRequest(const Request& request) {
        if (request.floor > currentFloor) {
            upStops.insert(request.floor);
        } else if (request.floor < currentFloor) {
            downStops.insert(request.floor);
        }
    }

    void run() {
        while (running) {
            step();
            std::this_thread::sleep_for(std::chrono::milliseconds(150));  // simulate
        }
    }

    void step() {
        Direction dir = direction;
        if (dir == Direction::UP) {
            if (upStops.empty()) {
                direction = downStops.empty() ? Direction::IDLE : Direction::DOWN;
                state = downStops.empty() ? ElevatorStates::IDLE : ElevatorStates::MOVING;
                return;
            }
            if (currentFloor == *upStops.begin()) {
                upStops.erase(upStops.begin());
                // STOPPING the Elevator
                // 3-4 lines of logic for opening/closing the doors
            }
            currentFloor++;
        } else if (dir == Direction::DOWN) {
            if (downStops.empty()) {
                direction = upStops.empty() ? Direction::IDLE : Direction::UP;
                state = upStops.empty() ? ElevatorStates::IDLE : ElevatorStates::MOVING;
                return;
            }
            if (currentFloor == *downStops.begin()) {
                downStops.erase(downStops.begin());
                // STOPPING the Elevator
                // 3-4 lines of logic for opening/closing the doors
            }
            currentFloor--;
        } else {
            state = ElevatorStates::IDLE;
        }
    }
};

class ElevatorSystem {
public:
    std::vector<std::unique_ptr<Elevator>> elevators;
    std::vector<std::thread> threads;
    std::unique_ptr<SchedulingStrategy> strategy;

    int numberOfElevators = 5;

    ElevatorSystem() {
        for (int i = 1; i <= 5; i++) {
            elevators.push_back(std::make_unique<Elevator>());
        }
    }

    void addRequest(const Request& request) {
        Elevator* elevator = strategy->getElevator(elevators, request);
        elevator->addRequest(request);
    }
};

class ElevatorSystemDemo {
};
