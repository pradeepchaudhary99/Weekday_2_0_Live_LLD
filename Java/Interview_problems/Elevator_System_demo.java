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
       lock since requests can arrive concurrently from many callers.
    3. No duplicate requests: stop floors are kept in a Set (TreeSet), so
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
    Each Elevator keeps two TreeSets: upStops (visited ascending, so
    `first()` is always the next stop while moving up) and downStops
    (visited descending, so `last()` is always the next stop while moving
    down). One `step()` call either advances one floor or services a stop;
    the background thread just calls step() on a fixed cadence -- this is
    what turns "the elevator has some destinations" into actual motion
    without busy-polling a shared queue.

Core Entities:
    Direction            -- UP, DOWN, IDLE
    ElevatorState         -- MOVING, STOPPED, IDLE, MAINTENANCE
    DoorState             -- OPEN, CLOSED
    Request               -- floor (+ direction, for external hall calls)
    SchedulingStrategy     -- assignment interface
    NearestElevatorStrategy
    Elevator (Runnable)   -- one cabin's state machine + movement thread
    ElevatorSystem         -- owns the fleet, routes requests via the strategy
================================================================================
*/

import java.util.ArrayList;
import java.util.List;
import java.util.TreeSet;

enum Direction {
    UP, DOWN, IDLE
}

enum ElevatorState {
    MOVING, STOPPED, IDLE, MAINTENANCE
}

enum DoorState {
    OPEN, CLOSED
}

class Request {
    final int floor;
    final Direction direction; // null for an internal cabin request

    Request(int floor) {
        this(floor, null);
    }

    Request(int floor, Direction direction) {
        this.floor = floor;
        this.direction = direction;
    }
}

interface SchedulingStrategy {
    Elevator getElevator(List<Elevator> elevators, Request request);
}

// Picks whichever elevator is numerically closest to the requested floor,
// with a preference for elevators that are currently idle (an idle
// elevator can retarget immediately; a moving one should ideally finish
// its current direction first, which a fancier SCAN-style strategy would
// model -- left as an extension point).
class NearestElevatorStrategy implements SchedulingStrategy {
    @Override
    public Elevator getElevator(List<Elevator> elevators, Request request) {
        Elevator best = null;
        int bestScore = Integer.MAX_VALUE;
        for (Elevator elevator : elevators) {
            int floor = elevator.getCurrentFloor();
            boolean idle = elevator.isIdle();
            int distance = Math.abs(floor - request.floor);
            // Idle elevators are scored as if they were half as far away,
            // so a busy elevator only wins when it is clearly closer.
            int score = idle ? distance : distance * 2;
            if (score < bestScore) {
                bestScore = score;
                best = elevator;
            }
        }
        return best;
    }
}

class Elevator implements Runnable {
    final int id;
    private final Object lock = new Object();
    private ElevatorState state = ElevatorState.IDLE;
    private DoorState doorState = DoorState.CLOSED;
    private int currentFloor;
    private Direction direction = Direction.IDLE;
    private final TreeSet<Integer> upStops = new TreeSet<>();   // visited ascending
    private final TreeSet<Integer> downStops = new TreeSet<>(); // visited descending (via last())
    private volatile boolean running = false;

    Elevator(int id, int startingFloor) {
        this.id = id;
        this.currentFloor = startingFloor;
    }

    void startElevator() {
        running = true;
    }

    void stopElevator() {
        running = false;
    }

    int getCurrentFloor() {
        synchronized (lock) {
            return currentFloor;
        }
    }

    boolean isIdle() {
        synchronized (lock) {
            return direction == Direction.IDLE && upStops.isEmpty() && downStops.isEmpty();
        }
    }

    void addRequest(int floor) {
        synchronized (lock) {
            if (floor > currentFloor) {
                upStops.add(floor);
                if (direction == Direction.IDLE) {
                    direction = Direction.UP;
                }
            } else if (floor < currentFloor) {
                downStops.add(floor);
                if (direction == Direction.IDLE) {
                    direction = Direction.DOWN;
                }
            }
            // floor == currentFloor: already there, nothing to queue.
        }
    }

    @Override
    public void run() {
        while (running) {
            step();
            try {
                Thread.sleep(50); // simulate the time to travel one floor
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
                return;
            }
        }
    }

    // Advances the elevator by exactly one unit of work: either it opens
    // its doors for a stop it has just reached, or it moves one floor
    // toward the next stop, or -- with nothing left to do -- it goes idle.
    void step() {
        synchronized (lock) {
            if (direction == Direction.UP) {
                if (upStops.isEmpty()) {
                    direction = downStops.isEmpty() ? Direction.IDLE : Direction.DOWN;
                    state = direction == Direction.IDLE ? ElevatorState.IDLE : ElevatorState.MOVING;
                    return;
                }
                if (currentFloor == upStops.first()) {
                    upStops.pollFirst();
                    openAndCloseDoors();
                    return;
                }
                state = ElevatorState.MOVING;
                currentFloor++;
            } else if (direction == Direction.DOWN) {
                if (downStops.isEmpty()) {
                    direction = upStops.isEmpty() ? Direction.IDLE : Direction.UP;
                    state = direction == Direction.IDLE ? ElevatorState.IDLE : ElevatorState.MOVING;
                    return;
                }
                if (currentFloor == downStops.last()) {
                    downStops.pollLast();
                    openAndCloseDoors();
                    return;
                }
                state = ElevatorState.MOVING;
                currentFloor--;
            } else {
                state = ElevatorState.IDLE;
            }
        }
    }

    // Called with `lock` already held.
    private void openAndCloseDoors() {
        state = ElevatorState.STOPPED;
        doorState = DoorState.OPEN;
        System.out.println("Elevator " + id + " arrived at floor " + currentFloor + ", doors OPEN");
        doorState = DoorState.CLOSED;
        System.out.println("Elevator " + id + " doors CLOSED at floor " + currentFloor);
    }
}

class ElevatorSystem {
    private final List<Elevator> elevators = new ArrayList<>();
    private final List<Thread> threads = new ArrayList<>();
    private final SchedulingStrategy strategy;

    ElevatorSystem(int numberOfElevators, SchedulingStrategy strategy) {
        this.strategy = strategy;
        for (int i = 1; i <= numberOfElevators; i++) {
            Elevator elevator = new Elevator(i, 0);
            elevator.startElevator();
            Thread thread = new Thread(elevator, "elevator-" + i);
            elevators.add(elevator);
            threads.add(thread);
            thread.start();
        }
    }

    void addRequest(Request request) {
        Elevator elevator = strategy.getElevator(elevators, request);
        elevator.addRequest(request.floor);
        System.out.println("Assigned request for floor " + request.floor + " to elevator " + elevator.id);
    }

    // No busy-polling on the caller's side either: sleep between checks
    // instead of spinning, and give up after timeoutMs so a bug in one
    // elevator can't hang the demo forever.
    boolean awaitAllIdle(long timeoutMs) throws InterruptedException {
        long deadline = System.currentTimeMillis() + timeoutMs;
        while (System.currentTimeMillis() < deadline) {
            boolean allIdle = true;
            for (Elevator elevator : elevators) {
                if (!elevator.isIdle()) {
                    allIdle = false;
                    break;
                }
            }
            if (allIdle) {
                return true;
            }
            Thread.sleep(20);
        }
        return false;
    }

    void printFleetStatus() {
        for (Elevator elevator : elevators) {
            System.out.println("  Elevator " + elevator.id + " resting at floor " + elevator.getCurrentFloor());
        }
    }

    void shutdown() throws InterruptedException {
        for (Elevator elevator : elevators) {
            elevator.stopElevator();
        }
        for (Thread thread : threads) {
            thread.join();
        }
    }
}

public class Elevator_System_demo {
    public static void main(String[] args) throws InterruptedException {
        ElevatorSystem system = new ElevatorSystem(3, new NearestElevatorStrategy());

        system.addRequest(new Request(5, Direction.UP));
        system.addRequest(new Request(2, Direction.UP));
        system.addRequest(new Request(8, Direction.DOWN));
        system.addRequest(new Request(1, Direction.DOWN));

        boolean finished = system.awaitAllIdle(5000);
        System.out.println("\nAll requests serviced: " + finished);
        System.out.println("Final fleet status:");
        system.printFleetStatus();

        system.shutdown();
    }
}
