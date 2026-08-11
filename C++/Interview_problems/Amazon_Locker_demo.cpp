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

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

enum class Size { SMALL = 1, MEDIUM = 2, LARGE = 3 };
enum class LockerStatus { AVAILABLE, RESERVED, OCCUPIED, OUT_OF_SERVICE };
enum class PackageStatus { CREATED, ASSIGNED, DELIVERED, PICKED_UP, EXPIRED, RETURNED };

std::string sizeName(Size size) {
    switch (size) {
        case Size::SMALL: return "SMALL";
        case Size::MEDIUM: return "MEDIUM";
        case Size::LARGE: return "LARGE";
    }
    return "UNKNOWN";
}

class Package {
public:
    std::string id;
    Size size;
    std::string address;
    PackageStatus status = PackageStatus::CREATED;
    std::optional<std::string> pickupCode;

    Package(std::string id, Size size, std::string address)
        : id(std::move(id)), size(size), address(std::move(address)) {}
};

class Locker {
public:
    std::string id;
    Size size;
    LockerStatus status = LockerStatus::AVAILABLE;
    std::shared_ptr<Package> currentPackage;

    Locker(std::string id, Size size) : id(std::move(id)), size(size) {}

    bool canFit(const Package& package) const {
        return status == LockerStatus::AVAILABLE && static_cast<int>(size) >= static_cast<int>(package.size);
    }

    void storePackage(std::shared_ptr<Package> package, const std::string& code) {
        currentPackage = package;
        pickupCode_ = code;
        status = LockerStatus::OCCUPIED;
        currentPackage->status = PackageStatus::DELIVERED;
        currentPackage->pickupCode = code;
    }

    std::shared_ptr<Package> pickup(const std::string& code) {
        if (!currentPackage || code != pickupCode_) {
            return nullptr;
        }
        auto package = currentPackage;
        package->status = PackageStatus::PICKED_UP;
        currentPackage = nullptr;
        pickupCode_.reset();
        status = LockerStatus::AVAILABLE;
        return package;
    }

private:
    std::optional<std::string> pickupCode_;
};

struct LockerAllocationStrategy {
    virtual ~LockerAllocationStrategy() = default;
    virtual std::shared_ptr<Locker> assignLocker(const std::vector<std::shared_ptr<Locker>>& lockers,
                                                   const Package& package) const = 0;
};

class SmallestFitAllocationStrategy : public LockerAllocationStrategy {
public:
    std::shared_ptr<Locker> assignLocker(const std::vector<std::shared_ptr<Locker>>& lockers,
                                          const Package& package) const override {
        std::shared_ptr<Locker> best = nullptr;
        for (const auto& locker : lockers) {
            if (!locker->canFit(package)) continue;
            if (!best || locker->size < best->size) {
                best = locker;
            }
        }
        return best;
    }
};

struct PickupCodeGenerationStrategy {
    virtual ~PickupCodeGenerationStrategy() = default;
    virtual std::string generate() const = 0;
};

class OtpPickupCodeStrategy : public PickupCodeGenerationStrategy {
public:
    std::string generate() const override {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 999999);
        char buffer[7];
        std::snprintf(buffer, sizeof(buffer), "%06d", dist(rng));
        return buffer;
    }
};

class LockerSystemManager {
public:
    void addLocker(std::shared_ptr<Locker> locker) {
        lockers_.push_back(locker);
        lockersById_[locker->id] = locker;
    }

    const std::vector<std::shared_ptr<Locker>>& allLockers() const { return lockers_; }

    std::shared_ptr<Locker> getLocker(const std::string& lockerId) const {
        auto it = lockersById_.find(lockerId);
        return it != lockersById_.end() ? it->second : nullptr;
    }

private:
    std::vector<std::shared_ptr<Locker>> lockers_;
    std::map<std::string, std::shared_ptr<Locker>> lockersById_;
};

class NoLockerAvailableError : public std::runtime_error {
public:
    explicit NoLockerAvailableError(const std::string& message) : std::runtime_error(message) {}
};

class InvalidPickupCodeError : public std::runtime_error {
public:
    explicit InvalidPickupCodeError(const std::string& message) : std::runtime_error(message) {}
};

class AmazonLockerManager {
public:
    AmazonLockerManager(std::shared_ptr<LockerSystemManager> systemManager,
                         std::shared_ptr<LockerAllocationStrategy> allocationStrategy,
                         std::shared_ptr<PickupCodeGenerationStrategy> pickupCodeStrategy)
        : systemManager_(std::move(systemManager)), allocationStrategy_(std::move(allocationStrategy)),
          pickupCodeStrategy_(std::move(pickupCodeStrategy)) {}

    std::shared_ptr<Locker> registerPackage(std::shared_ptr<Package> package) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto locker = allocationStrategy_->assignLocker(systemManager_->allLockers(), *package);
        if (!locker) {
            throw NoLockerAvailableError("No locker available for package " + package->id +
                                          " (size " + sizeName(package->size) + ")");
        }
        package->status = PackageStatus::ASSIGNED;
        std::string code = pickupCodeStrategy_->generate();
        locker->storePackage(package, code);
        packages_[package->id] = package;
        return locker;
    }

    std::shared_ptr<Package> claimPackage(const std::string& lockerId, const std::string& pickupCode) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto locker = systemManager_->getLocker(lockerId);
        if (!locker) {
            throw std::invalid_argument("Unknown locker " + lockerId);
        }
        auto package = locker->pickup(pickupCode);
        if (!package) {
            throw InvalidPickupCodeError("Invalid pickup code for locker " + lockerId);
        }
        return package;
    }

    std::optional<PackageStatus> packageStatus(const std::string& packageId) const {
        auto it = packages_.find(packageId);
        if (it == packages_.end()) return std::nullopt;
        return it->second->status;
    }

    std::optional<LockerStatus> lockerStatus(const std::string& lockerId) const {
        auto locker = systemManager_->getLocker(lockerId);
        if (!locker) return std::nullopt;
        return locker->status;
    }

private:
    std::shared_ptr<LockerSystemManager> systemManager_;
    std::shared_ptr<LockerAllocationStrategy> allocationStrategy_;
    std::shared_ptr<PickupCodeGenerationStrategy> pickupCodeStrategy_;
    std::map<std::string, std::shared_ptr<Package>> packages_;
    std::mutex mutex_;
};

std::string packageStatusName(PackageStatus status) {
    switch (status) {
        case PackageStatus::CREATED: return "CREATED";
        case PackageStatus::ASSIGNED: return "ASSIGNED";
        case PackageStatus::DELIVERED: return "DELIVERED";
        case PackageStatus::PICKED_UP: return "PICKED_UP";
        case PackageStatus::EXPIRED: return "EXPIRED";
        case PackageStatus::RETURNED: return "RETURNED";
    }
    return "UNKNOWN";
}

std::string lockerStatusName(LockerStatus status) {
    switch (status) {
        case LockerStatus::AVAILABLE: return "AVAILABLE";
        case LockerStatus::RESERVED: return "RESERVED";
        case LockerStatus::OCCUPIED: return "OCCUPIED";
        case LockerStatus::OUT_OF_SERVICE: return "OUT_OF_SERVICE";
    }
    return "UNKNOWN";
}

int main() {
    auto systemManager = std::make_shared<LockerSystemManager>();
    for (int i = 1; i <= 2; ++i) {
        systemManager->addLocker(std::make_shared<Locker>("S" + std::to_string(i), Size::SMALL));
    }
    for (int i = 1; i <= 2; ++i) {
        systemManager->addLocker(std::make_shared<Locker>("M" + std::to_string(i), Size::MEDIUM));
    }
    systemManager->addLocker(std::make_shared<Locker>("L1", Size::LARGE));

    AmazonLockerManager manager(systemManager, std::make_shared<SmallestFitAllocationStrategy>(),
                                 std::make_shared<OtpPickupCodeStrategy>());

    auto smallPkg = std::make_shared<Package>("P1", Size::SMALL, "221B Baker Street");
    auto mediumPkg = std::make_shared<Package>("P2", Size::MEDIUM, "42 Wallaby Way");
    auto largePkg = std::make_shared<Package>("P3", Size::LARGE, "4 Privet Drive");

    std::cout << "Registering packages:\n";
    for (const auto& pkg : {smallPkg, mediumPkg, largePkg}) {
        auto locker = manager.registerPackage(pkg);
        std::cout << "  " << pkg->id << " (" << sizeName(pkg->size) << ") -> locker " << locker->id
                  << ", pickup code " << *pkg->pickupCode << "\n";
    }

    std::cout << "\nAttempting pickup with a wrong code:\n";
    try {
        manager.claimPackage("S1", "000000");
    } catch (const InvalidPickupCodeError& e) {
        std::cout << "  " << e.what() << "\n";
    }

    std::cout << "\nPicking up with the correct code:\n";
    auto claimed = manager.claimPackage("S1", *smallPkg->pickupCode);
    std::cout << "  Claimed " << claimed->id << ", status now " << packageStatusName(claimed->status) << "\n";

    std::cout << "\nLocker S1 status: " << lockerStatusName(*manager.lockerStatus("S1")) << "\n";
    std::cout << "Package " << mediumPkg->id << " status: " << packageStatusName(*manager.packageStatus(mediumPkg->id)) << "\n";

    std::cout << "\nA second small package can now reuse the freed locker:\n";
    auto anotherSmall = std::make_shared<Package>("P4", Size::SMALL, "12 Grimmauld Place");
    auto locker = manager.registerPackage(anotherSmall);
    std::cout << "  " << anotherSmall->id << " (" << sizeName(anotherSmall->size) << ") -> locker " << locker->id
              << ", pickup code " << *anotherSmall->pickupCode << "\n";

    return 0;
}
