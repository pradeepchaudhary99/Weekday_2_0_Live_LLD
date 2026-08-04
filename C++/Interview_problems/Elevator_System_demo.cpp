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
       implementation, not a rewrite of ElevatorSystem.
    2. Thread-safety: each elevator runs on its own thread and mutates its
       own stop-queues/state; those fields are guarded by a per-elevator
       mutex since requests can arrive concurrently from many callers.
    3. No duplicate requests: stop floors are kept in a std::set, so
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
    Each Elevator keeps two sets: upStops (visited ascending, so
    `*begin()` is always the next stop while moving up) and downStops
    (visited descending, so `*rbegin()` is always the next stop while
    moving down). One step() call either advances one floor or services a
    stop; the background thread just calls step() on a fixed cadence --
    this is what turns "the elevator has some destinations" into actual
    motion without busy-polling a shared queue.

Core Entities:
    Direction            -- UP, DOWN, IDLE
    ElevatorState         -- MOVING, STOPPED, IDLE, MAINTENANCE
    DoorState             -- OPEN, CLOSED
    Request               -- floor (+ direction, for external hall calls)
    SchedulingStrategy     -- assignment interface
    NearestElevatorStrategy
    Elevator              -- one cabin's state machine + movement thread
    ElevatorSystem         -- owns the fleet, routes requests via the strategy
================================================================================
*/

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <thread>
#include <vector>

// std::cout is safe from internal corruption under concurrent use, but
// nothing stops two threads' output from interleaving mid-line. Every
// elevator runs on its own thread, so all console output funnels through
// this helper to keep each line intact and printed as one atomic unit.
std::mutex g_coutMutex;
void printLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(g_coutMutex);
    std::cout << line << std::endl;
}

enum class Direction { UP, DOWN, IDLE };

enum class ElevatorState { MOVING, STOPPED, IDLE, MAINTENANCE };

enum class DoorState { OPEN, CLOSED };

struct Request {
    int floor;
    std::optional<Direction> direction;  // nullopt for an internal cabin request

    explicit Request(int floor, std::optional<Direction> direction = std::nullopt)
        : floor(floor), direction(direction) {}
};

class Elevator;

struct SchedulingStrategy {
    virtual Elevator* getElevator(std::vector<std::unique_ptr<Elevator>>& elevators, const Request& request) = 0;
    virtual ~SchedulingStrategy() = default;
};

class Elevator {
public:
    int id;
    Elevator(int id, int startingFloor) : id(id), currentFloor_(startingFloor) {}

    void startElevator() { running_ = true; }

    void stopElevator() { running_ = false; }

    int getCurrentFloor() {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentFloor_;
    }

    bool isIdle() {
        std::lock_guard<std::mutex> lock(mutex_);
        return direction_ == Direction::IDLE && upStops_.empty() && downStops_.empty();
    }

    void addRequest(int floor) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (floor > currentFloor_) {
            upStops_.insert(floor);
            if (direction_ == Direction::IDLE) {
                direction_ = Direction::UP;
            }
        } else if (floor < currentFloor_) {
            downStops_.insert(floor);
            if (direction_ == Direction::IDLE) {
                direction_ = Direction::DOWN;
            }
        }
        // floor == currentFloor_: already there, nothing to queue.
    }

    void run() {
        while (running_) {
            step();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));  // simulate the time to travel one floor
        }
    }

    // Advances the elevator by exactly one unit of work: either it opens
    // its doors for a stop it has just reached, or it moves one floor
    // toward the next stop, or -- with nothing left to do -- it goes idle.
    void step() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (direction_ == Direction::UP) {
            if (upStops_.empty()) {
                direction_ = downStops_.empty() ? Direction::IDLE : Direction::DOWN;
                state_ = direction_ == Direction::IDLE ? ElevatorState::IDLE : ElevatorState::MOVING;
                return;
            }
            if (currentFloor_ == *upStops_.begin()) {
                upStops_.erase(upStops_.begin());
                openAndCloseDoors();
                return;
            }
            state_ = ElevatorState::MOVING;
            currentFloor_++;
        } else if (direction_ == Direction::DOWN) {
            if (downStops_.empty()) {
                direction_ = upStops_.empty() ? Direction::IDLE : Direction::UP;
                state_ = direction_ == Direction::IDLE ? ElevatorState::IDLE : ElevatorState::MOVING;
                return;
            }
            if (currentFloor_ == *downStops_.rbegin()) {
                downStops_.erase(std::prev(downStops_.end()));
                openAndCloseDoors();
                return;
            }
            state_ = ElevatorState::MOVING;
            currentFloor_--;
        } else {
            state_ = ElevatorState::IDLE;
        }
    }

private:
    // Called with mutex_ already held.
    void openAndCloseDoors() {
        state_ = ElevatorState::STOPPED;
        doorState_ = DoorState::OPEN;
        {
            std::ostringstream line;
            line << "Elevator " << id << " arrived at floor " << currentFloor_ << ", doors OPEN";
            printLine(line.str());
        }
        doorState_ = DoorState::CLOSED;
        {
            std::ostringstream line;
            line << "Elevator " << id << " doors CLOSED at floor " << currentFloor_;
            printLine(line.str());
        }
    }

    std::mutex mutex_;
    ElevatorState state_ = ElevatorState::IDLE;
    DoorState doorState_ = DoorState::CLOSED;
    int currentFloor_;
    Direction direction_ = Direction::IDLE;
    std::set<int> upStops_;                       // visited ascending
    std::set<int> downStops_;                     // visited descending (via rbegin())
    std::atomic<bool> running_{false};
};

// Picks whichever elevator is numerically closest to the requested floor,
// with a preference for elevators that are currently idle (an idle
// elevator can retarget immediately; a moving one should ideally finish
// its current direction first, which a fancier SCAN-style strategy would
// model -- left as an extension point).
class NearestElevatorStrategy : public SchedulingStrategy {
public:
    Elevator* getElevator(std::vector<std::unique_ptr<Elevator>>& elevators, const Request& request) override {
        Elevator* best = nullptr;
        int bestScore = std::numeric_limits<int>::max();
        for (auto& elevator : elevators) {
            int floor = elevator->getCurrentFloor();
            bool idle = elevator->isIdle();
            int distance = std::abs(floor - request.floor);
            // Idle elevators are scored as if they were half as far away,
            // so a busy elevator only wins when it is clearly closer.
            int score = idle ? distance : distance * 2;
            if (score < bestScore) {
                bestScore = score;
                best = elevator.get();
            }
        }
        return best;
    }
};

class ElevatorSystem {
public:
    ElevatorSystem(int numberOfElevators, std::unique_ptr<SchedulingStrategy> strategy)
        : strategy_(std::move(strategy)) {
        for (int i = 1; i <= numberOfElevators; i++) {
            auto elevator = std::make_unique<Elevator>(i, 0);
            elevator->startElevator();
            Elevator* raw = elevator.get();
            elevators_.push_back(std::move(elevator));
            threads_.emplace_back([raw] { raw->run(); });
        }
    }

    void addRequest(const Request& request) {
        Elevator* elevator = strategy_->getElevator(elevators_, request);
        elevator->addRequest(request.floor);
        std::ostringstream line;
        line << "Assigned request for floor " << request.floor << " to elevator " << elevator->id;
        printLine(line.str());
    }

    // No busy-polling on the caller's side either: sleep between checks
    // instead of spinning, and give up after timeoutMs so a bug in one
    // elevator can't hang the demo forever.
    bool awaitAllIdle(int timeoutMs) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline) {
            bool allIdle = std::all_of(elevators_.begin(), elevators_.end(),
                                        [](auto& elevator) { return elevator->isIdle(); });
            if (allIdle) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return false;
    }

    void printFleetStatus() {
        for (auto& elevator : elevators_) {
            std::ostringstream line;
            line << "  Elevator " << elevator->id << " resting at floor " << elevator->getCurrentFloor();
            printLine(line.str());
        }
    }

    void shutdown() {
        for (auto& elevator : elevators_) {
            elevator->stopElevator();
        }
        for (auto& thread : threads_) {
            thread.join();
        }
    }

private:
    std::vector<std::unique_ptr<Elevator>> elevators_;
    std::vector<std::thread> threads_;
    std::unique_ptr<SchedulingStrategy> strategy_;
};

int main() {
    ElevatorSystem system(3, std::make_unique<NearestElevatorStrategy>());

    system.addRequest(Request(5, Direction::UP));
    system.addRequest(Request(2, Direction::UP));
    system.addRequest(Request(8, Direction::DOWN));
    system.addRequest(Request(1, Direction::DOWN));

    bool finished = system.awaitAllIdle(5000);
    printLine(std::string("\nAll requests serviced: ") + (finished ? "true" : "false"));
    printLine("Final fleet status:");
    system.printFleetStatus();

    system.shutdown();
    return 0;
}
