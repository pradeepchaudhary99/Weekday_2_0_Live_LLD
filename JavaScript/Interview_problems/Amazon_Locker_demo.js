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
    1. Thread-safety (moot for Node's single-threaded event loop, but the
       design would carry over to a worker-thread model).
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

const Size = Object.freeze({ SMALL: 1, MEDIUM: 2, LARGE: 3 });
const LockerStatus = Object.freeze({ AVAILABLE: "AVAILABLE", RESERVED: "RESERVED", OCCUPIED: "OCCUPIED", OUT_OF_SERVICE: "OUT_OF_SERVICE" });
const PackageStatus = Object.freeze({
    CREATED: "CREATED", ASSIGNED: "ASSIGNED", DELIVERED: "DELIVERED",
    PICKED_UP: "PICKED_UP", EXPIRED: "EXPIRED", RETURNED: "RETURNED",
});

function sizeName(size) {
    return Object.keys(Size).find((key) => Size[key] === size);
}

class Package {
    constructor(id, size, address) {
        this.id = id;
        this.size = size;
        this.address = address;
        this.status = PackageStatus.CREATED;
        this.pickupCode = null;
    }
}

class Locker {
    constructor(id, size) {
        this.id = id;
        this.size = size;
        this.status = LockerStatus.AVAILABLE;
        this.currentPackage = null;
        this._pickupCode = null;
    }

    canFit(pkg) {
        return this.status === LockerStatus.AVAILABLE && this.size >= pkg.size;
    }

    storePackage(pkg, pickupCode) {
        this.currentPackage = pkg;
        this._pickupCode = pickupCode;
        this.status = LockerStatus.OCCUPIED;
        pkg.status = PackageStatus.DELIVERED;
        pkg.pickupCode = pickupCode;
    }

    pickup(pickupCode) {
        if (!this.currentPackage || pickupCode !== this._pickupCode) {
            return null;
        }
        const pkg = this.currentPackage;
        pkg.status = PackageStatus.PICKED_UP;
        this.currentPackage = null;
        this._pickupCode = null;
        this.status = LockerStatus.AVAILABLE;
        return pkg;
    }
}

class LockerAllocationStrategy {
    assignLocker(_lockers, _pkg) {
        throw new Error("assignLocker must be implemented by subclasses");
    }
}

class SmallestFitAllocationStrategy extends LockerAllocationStrategy {
    assignLocker(lockers, pkg) {
        const candidates = lockers.filter((locker) => locker.canFit(pkg));
        if (candidates.length === 0) {
            return null;
        }
        return candidates.reduce((best, locker) => (locker.size < best.size ? locker : best));
    }
}

class PickupCodeGenerationStrategy {
    generate() {
        throw new Error("generate must be implemented by subclasses");
    }
}

class OtpPickupCodeStrategy extends PickupCodeGenerationStrategy {
    generate() {
        return String(Math.floor(Math.random() * 1000000)).padStart(6, "0");
    }
}

class LockerSystemManager {
    constructor() {
        this._lockers = new Map();
    }

    addLocker(locker) {
        this._lockers.set(locker.id, locker);
    }

    allLockers() {
        return Array.from(this._lockers.values());
    }

    getLocker(lockerId) {
        return this._lockers.get(lockerId) ?? null;
    }
}

class NoLockerAvailableError extends Error {}
class InvalidPickupCodeError extends Error {}

class AmazonLockerManager {
    constructor(systemManager, allocationStrategy, pickupCodeStrategy) {
        this._systemManager = systemManager;
        this._allocationStrategy = allocationStrategy;
        this._pickupCodeStrategy = pickupCodeStrategy;
        this._packages = new Map();
    }

    registerPackage(pkg) {
        const locker = this._allocationStrategy.assignLocker(this._systemManager.allLockers(), pkg);
        if (!locker) {
            throw new NoLockerAvailableError(`No locker available for package ${pkg.id} (size ${sizeName(pkg.size)})`);
        }
        pkg.status = PackageStatus.ASSIGNED;
        const code = this._pickupCodeStrategy.generate();
        locker.storePackage(pkg, code);
        this._packages.set(pkg.id, pkg);
        return locker;
    }

    claimPackage(lockerId, pickupCode) {
        const locker = this._systemManager.getLocker(lockerId);
        if (!locker) {
            throw new Error(`Unknown locker ${lockerId}`);
        }
        const pkg = locker.pickup(pickupCode);
        if (!pkg) {
            throw new InvalidPickupCodeError(`Invalid pickup code for locker ${lockerId}`);
        }
        return pkg;
    }

    packageStatus(packageId) {
        const pkg = this._packages.get(packageId);
        return pkg ? pkg.status : null;
    }

    lockerStatus(lockerId) {
        const locker = this._systemManager.getLocker(lockerId);
        return locker ? locker.status : null;
    }
}

function main() {
    const systemManager = new LockerSystemManager();
    for (let i = 1; i <= 2; i++) {
        systemManager.addLocker(new Locker(`S${i}`, Size.SMALL));
    }
    for (let i = 1; i <= 2; i++) {
        systemManager.addLocker(new Locker(`M${i}`, Size.MEDIUM));
    }
    systemManager.addLocker(new Locker("L1", Size.LARGE));

    const manager = new AmazonLockerManager(systemManager, new SmallestFitAllocationStrategy(), new OtpPickupCodeStrategy());

    const smallPkg = new Package("P1", Size.SMALL, "221B Baker Street");
    const mediumPkg = new Package("P2", Size.MEDIUM, "42 Wallaby Way");
    const largePkg = new Package("P3", Size.LARGE, "4 Privet Drive");

    console.log("Registering packages:");
    for (const pkg of [smallPkg, mediumPkg, largePkg]) {
        const locker = manager.registerPackage(pkg);
        console.log(`  ${pkg.id} (${sizeName(pkg.size)}) -> locker ${locker.id}, pickup code ${pkg.pickupCode}`);
    }

    console.log("\nAttempting pickup with a wrong code:");
    try {
        manager.claimPackage("S1", "000000");
    } catch (e) {
        if (e instanceof InvalidPickupCodeError) {
            console.log(`  ${e.message}`);
        } else {
            throw e;
        }
    }

    console.log("\nPicking up with the correct code:");
    const claimed = manager.claimPackage("S1", smallPkg.pickupCode);
    console.log(`  Claimed ${claimed.id}, status now ${claimed.status}`);

    console.log(`\nLocker S1 status: ${manager.lockerStatus("S1")}`);
    console.log(`Package ${mediumPkg.id} status: ${manager.packageStatus(mediumPkg.id)}`);

    console.log("\nA second small package can now reuse the freed locker:");
    const anotherSmall = new Package("P4", Size.SMALL, "12 Grimmauld Place");
    const locker = manager.registerPackage(anotherSmall);
    console.log(`  ${anotherSmall.id} (${sizeName(anotherSmall.size)}) -> locker ${locker.id}, pickup code ${anotherSmall.pickupCode}`);
}

main();
