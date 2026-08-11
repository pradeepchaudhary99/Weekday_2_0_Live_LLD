



/*
    ---> Functional Requirements:

    Amazon Locker  ---> One Big Box(Vending machine)
        Notification System     

        Locker
        register package 
        Find suitable Locker    (Delivery partner)
        claim package
        generate pickup code 
            -- OTP 
            -- QR
        Query package/locker status 

    Thread Safety 
    Maintability 
    Performance


    Core Entities: 

    AmazonLockerManager 
    LockerSystem
    Locker
    Package
    PickupCodeGenerationStrategy 
    LockerAllocationStrategy


*/


enum Size {
    SMALL,
    MEDIUM,
    LARGE
}

enum LockerStatus{
    AVAILABLE,
    RESERVED,
    OCCUPIED,
    OUT_OF_SERVICE
}

enum PackageStatus{
    CREATED,
    ASSIGNED,
    DELIVERED,
    PICK_UP,
    EXPIRED,
    RETURNED
}


class Package{
    id;
    size;
    address;
    meta_data;
    PackageStatus status;
}

class Locker{
    id;
    size;
    status;
    Package currentPackage;
    String pickupCode; 
    Time packageAssignedTime;
    
    doorOpenClose();
    canFit(Package pkg);
    reserve();
    release();
    storePackage(Package pkg, String pickupCode);
    pickup(String pickupCode);
}


interface PickupCodeGenerationStrategy{
    String generate();
}

interface LockerAllocationStrategy{
    Locker assignLocker();
}

class LockerSystemManager{
    Map<Size, Queue<Locker>> availableLockers;
    addLocker(Locker locker);
    Locker getLocker(Package pkg, LockerAllocationStrategy strategy);   //deiveryGuy
    Package claimPackage(String pickupCode); //User 
}


class AmazonLocker{
    LockerSystemManager lockManager;
    LockerAllocationStrategy strategy;
    PickupCodeGenerationStrategy pickCodeGenerationStrategy;
    Package claimPackage(String pickupCode);
    Locker assignLocker(Package package);
}

// User 

// Delivery Agennt 








public class Amazon_Locker_demo {
    
}
