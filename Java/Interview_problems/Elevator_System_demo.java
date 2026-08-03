

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

import java.util.List;
import java.util.TreeSet;

enum Direction{
    UP, DOWN, IDLE
}

enum ElevatorStates{
    MOVING,
    STOPPED,
    IDLE,
    MAINTAINENCE 
}

enum DoorState{
    OPEN, CLOSED
}

class Request{
    int floor;
    Direction direction;
}

interface SchedulingStrategy{
    Elevator getElevator(List<Elevator> elevators, Request request);
}

class NearestElevatorStrategy implements SchedulingStrategy{

}

class Elevator implements Runnable{
    UUID id;
    ElevatorStates state;
    DoorState doorstate; 
    int currentFloor;
    TreeSet<Integer> upstops;   // Increasing Order 
    TreeSet<Integer> downStops; // Decreasing Order
    Boolean running = false;
    Direction direction;

    void startElevator(){
        running = true;
    }

    void stopElevator(){
        running = false;
    }

    void addRequest(Request request){
        if(request.floor > currentFloor){
            upstops.add(request.floor);
        }
        else if(request.floor < currentFloor){
            downStops.add(request.floor);
        }
    }


    @Override
    public void run() {
        while(running){
            try{
                step();
                Thread.sleep(150); // simulate 
            }
        }
    }

    void step() {
        Direction dir = direction;
        if(dir == Direction.UP){
            if(upstops.isEmpty()){
                direction = downStops.isEmpty()? Direction.IDLE : Direction.DOWN;
                state = downStops.isEmpty()? ElevatorStates.IDLE : ElevatorStates.MOVING;
                return;
            }
            if(currentFloor == upstops.First()){
                upstops.pollFirst();
                // STOPPING the Elevator 
                // 3-4 lines of logic for opening/closing the doors 
            }
            currentFloor++;
        }else if(dir == Direction.DOWN){
            if(downStops.isEmpty()){
                direction = upstops.isEmpty()? Direction.IDLE : Direction.UP;
                state = upstops.isEmpty()? ElevatorStates.IDLE : ElevatorStates.MOVING;
                return;
            }
            if(currentFloor == doorstate.First()){
                upstops.pollFirst();
                // STOPPING the Elevator 
                // 3-4 lines of logic for opening/closing the doors 
            }
            currentFloor--;

        }else{
            state = ElevatorStates.IDLE;
        }
    }
}


class ElevatorSystem{
    List<Elevator> elevators;
    List<Thread> threads; 
    SchedulingStrategy strategy;
    
    int numberOfElevators = 5;
    for(int i = 1; i <= 5; i++){
        Elevator elevator = new Elevator();
        Thread thread = new Thread(elevator);
        elevators.add(elevator);
        threads.add(thread);
    }


    void addRequest(Request request){
        Elevator elevator = strategy.getElevator(elevators, request);
        elevator.addRequest(request);
    }
}
















public class Elevator_System_demo {
    
}
