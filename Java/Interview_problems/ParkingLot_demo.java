

/*
parking Lot 

1. Parkinglot should be able to manager multiple floors 
2. Every floor can have any number of parkingSpot 
3. parkingLot should support different size/types of vechicles and parkingSpots 
4. ParkingLot should support multiple entry and exit gates 
5. Find available parking spots 
6. Support ticket generation at the entry of the vehicle 
7. Support payment Strategies at the exit of the vehicle


Core Entities: 
ParkingLotManager
parkingFloor 
ParkingSpot 
Vechicle 
Ticket 
PaymentStrategy 
PriceCalcualtionStrategy
EntryGate
ExitGate 


*/


enum VehicleType{
    BIKE,
    CAR,
    TRUCK
}

enum Spotype{
    BIKE,
    COMPACT,
    LARGE
}

enum TicketStatus{
    ACTIVE,
    PAID,
    CLOSED
}

class Vehicle{
    String licenseNumber;
    VehicleType type;
}

class Bike extends Vehicle;
class Car extends Vehicle;

class ParkingSpot{
    String id;
    Spottype type;
    Vehicle vechile;
    boolean isOccupied;
}

class ParkingFloor{
    int floorNumber;
    list<ParkingSpot> spots;
    /*
        Map<Size, List<ParkingSpot>> spots;
    */
   
}


public class ParkingLot_demo {
    
}
