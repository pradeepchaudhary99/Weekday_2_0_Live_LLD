/*
================================================================================
LLD: Amazon Locker
================================================================================

Functional Requirements:
    1. Register a package for delivery into a locker sized to fit it
       (a delivery agent drops the package into an assigned locker).
    2. Find a suitable free locker for a package via a pluggable
       allocation strategy.
    3. Generate a pickup code when a package is stored (OTP / QR).
    4. Let a user claim (pick up) a package by presenting the pickup code.
    5. Query package/locker status.

Non-Functional Requirements:
    1. Thread-safety.
    2. Maintainability / extensibility to new allocation and pickup-code
       strategies.
    3. Performance: locker lookup should not scan the whole system for
       every request.

Design:
    LockerAllocationStrategy (Strategy) picks a free locker for a
    package's size; SmallestFitAllocationStrategy returns the smallest
    available locker that is still big enough for the package, so large
    lockers stay free for large packages.

    PickupCodeGenerationStrategy (Strategy) produces the code a user must
    present at pickup; OtpPickupCodeStrategy generates a random 6-digit
    numeric OTP.

    LockerSystemManager owns the lockers and answers allocation queries.
    AmazonLockerManager (facade) wires the system manager and the two
    strategies together: registerPackage() finds+reserves a locker,
    generates a code, and stores the package; claimPackage() validates
    the presented code against the reserved locker and releases it.

Core Entities:
    Size, LockerStatus, PackageStatus (enums)
    Package
    Locker
    LockerAllocationStrategy / SmallestFitAllocationStrategy
    PickupCodeGenerationStrategy / OtpPickupCodeStrategy
    LockerSystemManager
    AmazonLockerManager
================================================================================
*/

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Random;

enum Size {
    SMALL(1),
    MEDIUM(2),
    LARGE(3);

    final int rank;

    Size(int rank) {
        this.rank = rank;
    }
}

enum LockerStatus {
    AVAILABLE,
    RESERVED,
    OCCUPIED,
    OUT_OF_SERVICE
}

enum PackageStatus {
    CREATED,
    ASSIGNED,
    DELIVERED,
    PICKED_UP,
    EXPIRED,
    RETURNED
}

class Package {
    final String id;
    final Size size;
    final String address;
    PackageStatus status = PackageStatus.CREATED;
    String pickupCode;

    Package(String id, Size size, String address) {
        this.id = id;
        this.size = size;
        this.address = address;
    }
}

class Locker {
    final String id;
    final Size size;
    LockerStatus status = LockerStatus.AVAILABLE;
    Package currentPackage;
    private String pickupCode;

    Locker(String id, Size size) {
        this.id = id;
        this.size = size;
    }

    boolean canFit(Package pkg) {
        return status == LockerStatus.AVAILABLE && size.rank >= pkg.size.rank;
    }

    void storePackage(Package pkg, String code) {
        this.currentPackage = pkg;
        this.pickupCode = code;
        this.status = LockerStatus.OCCUPIED;
        pkg.status = PackageStatus.DELIVERED;
        pkg.pickupCode = code;
    }

    Package pickup(String code) {
        if (currentPackage == null || !code.equals(pickupCode)) {
            return null;
        }
        Package pkg = currentPackage;
        pkg.status = PackageStatus.PICKED_UP;
        currentPackage = null;
        pickupCode = null;
        status = LockerStatus.AVAILABLE;
        return pkg;
    }
}

interface LockerAllocationStrategy {
    Locker assignLocker(List<Locker> lockers, Package pkg);
}

class SmallestFitAllocationStrategy implements LockerAllocationStrategy {
    @Override
    public Locker assignLocker(List<Locker> lockers, Package pkg) {
        Locker best = null;
        for (Locker locker : lockers) {
            if (!locker.canFit(pkg)) continue;
            if (best == null || locker.size.rank < best.size.rank) {
                best = locker;
            }
        }
        return best;
    }
}

interface PickupCodeGenerationStrategy {
    String generate();
}

class OtpPickupCodeStrategy implements PickupCodeGenerationStrategy {
    private final Random random = new Random();

    @Override
    public String generate() {
        return String.format("%06d", random.nextInt(1_000_000));
    }
}

class LockerSystemManager {
    private final Map<String, Locker> lockersById = new LinkedHashMap<>();

    void addLocker(Locker locker) {
        lockersById.put(locker.id, locker);
    }

    List<Locker> allLockers() {
        return new ArrayList<>(lockersById.values());
    }

    Locker getLocker(String lockerId) {
        return lockersById.get(lockerId);
    }
}

class NoLockerAvailableException extends RuntimeException {
    NoLockerAvailableException(String message) {
        super(message);
    }
}

class InvalidPickupCodeException extends RuntimeException {
    InvalidPickupCodeException(String message) {
        super(message);
    }
}

class AmazonLockerManager {
    private final LockerSystemManager systemManager;
    private final LockerAllocationStrategy allocationStrategy;
    private final PickupCodeGenerationStrategy pickupCodeStrategy;
    private final Map<String, Package> packages = new LinkedHashMap<>();
    private final Object lock = new Object();

    AmazonLockerManager(LockerSystemManager systemManager, LockerAllocationStrategy allocationStrategy,
                         PickupCodeGenerationStrategy pickupCodeStrategy) {
        this.systemManager = systemManager;
        this.allocationStrategy = allocationStrategy;
        this.pickupCodeStrategy = pickupCodeStrategy;
    }

    Locker registerPackage(Package pkg) {
        synchronized (lock) {
            Locker locker = allocationStrategy.assignLocker(systemManager.allLockers(), pkg);
            if (locker == null) {
                throw new NoLockerAvailableException(
                        "No locker available for package " + pkg.id + " (size " + pkg.size + ")");
            }
            pkg.status = PackageStatus.ASSIGNED;
            String code = pickupCodeStrategy.generate();
            locker.storePackage(pkg, code);
            packages.put(pkg.id, pkg);
            return locker;
        }
    }

    Package claimPackage(String lockerId, String pickupCode) {
        synchronized (lock) {
            Locker locker = systemManager.getLocker(lockerId);
            if (locker == null) {
                throw new IllegalArgumentException("Unknown locker " + lockerId);
            }
            Package pkg = locker.pickup(pickupCode);
            if (pkg == null) {
                throw new InvalidPickupCodeException("Invalid pickup code for locker " + lockerId);
            }
            return pkg;
        }
    }

    Optional<PackageStatus> packageStatus(String packageId) {
        Package pkg = packages.get(packageId);
        return pkg == null ? Optional.empty() : Optional.of(pkg.status);
    }

    Optional<LockerStatus> lockerStatus(String lockerId) {
        Locker locker = systemManager.getLocker(lockerId);
        return locker == null ? Optional.empty() : Optional.of(locker.status);
    }
}

public class Amazon_Locker_demo {
    public static void main(String[] args) {
        LockerSystemManager systemManager = new LockerSystemManager();
        for (int i = 1; i <= 2; i++) {
            systemManager.addLocker(new Locker("S" + i, Size.SMALL));
        }
        for (int i = 1; i <= 2; i++) {
            systemManager.addLocker(new Locker("M" + i, Size.MEDIUM));
        }
        systemManager.addLocker(new Locker("L1", Size.LARGE));

        AmazonLockerManager manager = new AmazonLockerManager(
                systemManager, new SmallestFitAllocationStrategy(), new OtpPickupCodeStrategy());

        Package smallPkg = new Package("P1", Size.SMALL, "221B Baker Street");
        Package mediumPkg = new Package("P2", Size.MEDIUM, "42 Wallaby Way");
        Package largePkg = new Package("P3", Size.LARGE, "4 Privet Drive");

        System.out.println("Registering packages:");
        for (Package pkg : List.of(smallPkg, mediumPkg, largePkg)) {
            Locker locker = manager.registerPackage(pkg);
            System.out.println("  " + pkg.id + " (" + pkg.size + ") -> locker " + locker.id
                    + ", pickup code " + pkg.pickupCode);
        }

        System.out.println("\nAttempting pickup with a wrong code:");
        try {
            manager.claimPackage("S1", "000000");
        } catch (InvalidPickupCodeException e) {
            System.out.println("  " + e.getMessage());
        }

        System.out.println("\nPicking up with the correct code:");
        Package claimed = manager.claimPackage("S1", smallPkg.pickupCode);
        System.out.println("  Claimed " + claimed.id + ", status now " + claimed.status);

        System.out.println("\nLocker S1 status: " + manager.lockerStatus("S1").get());
        System.out.println("Package " + mediumPkg.id + " status: " + manager.packageStatus(mediumPkg.id).get());

        System.out.println("\nA second small package can now reuse the freed locker:");
        Package anotherSmall = new Package("P4", Size.SMALL, "12 Grimmauld Place");
        Locker locker = manager.registerPackage(anotherSmall);
        System.out.println("  " + anotherSmall.id + " (" + anotherSmall.size + ") -> locker " + locker.id
                + ", pickup code " + anotherSmall.pickupCode);
    }
}
