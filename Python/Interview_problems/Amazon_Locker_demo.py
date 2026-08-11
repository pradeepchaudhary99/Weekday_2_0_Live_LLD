"""
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
    strategies together: register_package() finds+reserves a locker,
    generates a code, and stores the package; claim_package() validates
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
"""

import random
import threading
from abc import ABC, abstractmethod
from enum import Enum, auto
from typing import Dict, List, Optional


class Size(Enum):
    SMALL = 1
    MEDIUM = 2
    LARGE = 3


class LockerStatus(Enum):
    AVAILABLE = auto()
    RESERVED = auto()
    OCCUPIED = auto()
    OUT_OF_SERVICE = auto()


class PackageStatus(Enum):
    CREATED = auto()
    ASSIGNED = auto()
    DELIVERED = auto()
    PICKED_UP = auto()
    EXPIRED = auto()
    RETURNED = auto()


class Package:
    def __init__(self, id: str, size: Size, address: str):
        self.id = id
        self.size = size
        self.address = address
        self.status = PackageStatus.CREATED
        self.pickup_code: Optional[str] = None


class Locker:
    def __init__(self, id: str, size: Size):
        self.id = id
        self.size = size
        self.status = LockerStatus.AVAILABLE
        self.current_package: Optional[Package] = None
        self._pickup_code: Optional[str] = None

    def can_fit(self, package: Package) -> bool:
        return self.status == LockerStatus.AVAILABLE and self.size.value >= package.size.value

    def store_package(self, package: Package, pickup_code: str) -> None:
        self.current_package = package
        self._pickup_code = pickup_code
        self.status = LockerStatus.OCCUPIED
        package.status = PackageStatus.DELIVERED
        package.pickup_code = pickup_code

    def pickup(self, pickup_code: str) -> Optional[Package]:
        if self.current_package is None or pickup_code != self._pickup_code:
            return None
        package = self.current_package
        package.status = PackageStatus.PICKED_UP
        self.current_package = None
        self._pickup_code = None
        self.status = LockerStatus.AVAILABLE
        return package


class LockerAllocationStrategy(ABC):
    @abstractmethod
    def assign_locker(self, lockers: List[Locker], package: Package) -> Optional[Locker]:
        raise NotImplementedError


class SmallestFitAllocationStrategy(LockerAllocationStrategy):
    def assign_locker(self, lockers: List[Locker], package: Package) -> Optional[Locker]:
        candidates = [locker for locker in lockers if locker.can_fit(package)]
        if not candidates:
            return None
        return min(candidates, key=lambda locker: locker.size.value)


class PickupCodeGenerationStrategy(ABC):
    @abstractmethod
    def generate(self) -> str:
        raise NotImplementedError


class OtpPickupCodeStrategy(PickupCodeGenerationStrategy):
    def generate(self) -> str:
        return f"{random.randint(0, 999999):06d}"


class LockerSystemManager:
    def __init__(self):
        self._lockers: Dict[str, Locker] = {}

    def add_locker(self, locker: Locker) -> None:
        self._lockers[locker.id] = locker

    def all_lockers(self) -> List[Locker]:
        return list(self._lockers.values())

    def get_locker(self, locker_id: str) -> Optional[Locker]:
        return self._lockers.get(locker_id)


class NoLockerAvailableError(RuntimeError):
    pass


class InvalidPickupCodeError(RuntimeError):
    pass


class AmazonLockerManager:
    def __init__(self, system_manager: LockerSystemManager, allocation_strategy: LockerAllocationStrategy,
                 pickup_code_strategy: PickupCodeGenerationStrategy):
        self._system_manager = system_manager
        self._allocation_strategy = allocation_strategy
        self._pickup_code_strategy = pickup_code_strategy
        self._packages: Dict[str, Package] = {}
        self._lock = threading.Lock()

    def register_package(self, package: Package) -> Locker:
        with self._lock:
            locker = self._allocation_strategy.assign_locker(self._system_manager.all_lockers(), package)
            if locker is None:
                raise NoLockerAvailableError(f"No locker available for package {package.id} (size {package.size.name})")
            package.status = PackageStatus.ASSIGNED
            code = self._pickup_code_strategy.generate()
            locker.store_package(package, code)
            self._packages[package.id] = package
            return locker

    def claim_package(self, locker_id: str, pickup_code: str) -> Package:
        with self._lock:
            locker = self._system_manager.get_locker(locker_id)
            if locker is None:
                raise ValueError(f"Unknown locker {locker_id}")
            package = locker.pickup(pickup_code)
            if package is None:
                raise InvalidPickupCodeError(f"Invalid pickup code for locker {locker_id}")
            return package

    def package_status(self, package_id: str) -> Optional[PackageStatus]:
        package = self._packages.get(package_id)
        return package.status if package else None

    def locker_status(self, locker_id: str) -> Optional[LockerStatus]:
        locker = self._system_manager.get_locker(locker_id)
        return locker.status if locker else None


def main() -> None:
    system_manager = LockerSystemManager()
    for i in range(1, 3):
        system_manager.add_locker(Locker(f"S{i}", Size.SMALL))
    for i in range(1, 3):
        system_manager.add_locker(Locker(f"M{i}", Size.MEDIUM))
    system_manager.add_locker(Locker("L1", Size.LARGE))

    manager = AmazonLockerManager(system_manager, SmallestFitAllocationStrategy(), OtpPickupCodeStrategy())

    small_pkg = Package("P1", Size.SMALL, "221B Baker Street")
    medium_pkg = Package("P2", Size.MEDIUM, "42 Wallaby Way")
    large_pkg = Package("P3", Size.LARGE, "4 Privet Drive")

    print("Registering packages:")
    for pkg in (small_pkg, medium_pkg, large_pkg):
        locker = manager.register_package(pkg)
        print(f"  {pkg.id} ({pkg.size.name}) -> locker {locker.id}, pickup code {pkg.pickup_code}")

    print("\nAttempting pickup with a wrong code:")
    try:
        manager.claim_package("S1", "000000")
    except InvalidPickupCodeError as e:
        print(f"  {e}")

    print("\nPicking up with the correct code:")
    claimed = manager.claim_package("S1", small_pkg.pickup_code)
    print(f"  Claimed {claimed.id}, status now {claimed.status.name}")

    print(f"\nLocker S1 status: {manager.locker_status('S1').name}")
    print(f"Package {medium_pkg.id} status: {manager.package_status(medium_pkg.id).name}")

    print("\nA second small package can now reuse the freed locker:")
    another_small = Package("P4", Size.SMALL, "12 Grimmauld Place")
    locker = manager.register_package(another_small)
    print(f"  {another_small.id} ({another_small.size.name}) -> locker {locker.id}, pickup code {another_small.pickup_code}")


if __name__ == "__main__":
    main()
