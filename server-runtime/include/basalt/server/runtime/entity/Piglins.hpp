#pragma once

#include <basalt/server/runtime/entity/ai/BrainKit.hpp>
#include <basalt/server/runtime/entity/ai/DoorInteraction.hpp>
#include <basalt/server/world/loot/Loot.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace piglins {

inline constexpr std::uint8_t kBabyDataIndex = 17;
inline constexpr std::uint8_t kChargingDataIndex = 18;
inline constexpr std::uint8_t kDancingDataIndex = 19;

inline constexpr std::int32_t kAngerDuration = 600;
inline constexpr std::int32_t kAdmireDuration = 119;
inline constexpr std::int32_t kMaxDistanceToWalkToItem = 9;
inline constexpr std::int32_t kMaxTimeToWalkToItem = 200;
inline constexpr std::int32_t kDisableWalkToItem = 200;
inline constexpr std::int32_t kCelebrationTime = 300;
inline constexpr std::int32_t kHitByPlayerMemory = 400;
inline constexpr std::int32_t kBabyFleeAfterHit = 100;
inline constexpr std::int32_t kEatCooldown = 200;
inline constexpr std::int32_t kMeleeCooldown = 20;
inline constexpr std::size_t kInventorySize = 8;
inline constexpr std::array<std::int32_t, 2> kTimeBetweenHunts{30 * 20, 120 * 20};
inline constexpr std::array<std::int32_t, 2> kRetreatDuration{5 * 20, 20 * 20};
inline constexpr std::array<std::int32_t, 2> kAvoidZombified{5 * 20, 7 * 20};
inline constexpr std::array<std::int32_t, 2> kRideStartInterval{10 * 20, 40 * 20};
inline constexpr std::array<std::int32_t, 2> kRideDuration{10 * 20, 30 * 20};

enum class Activity : std::uint8_t { Idle, AdmireItem, Fight, Avoid, Celebrate, Ride };

enum class Crossbow : std::uint8_t { Uncharged, Charging, Charged, ReadyToAttack };

/// parity: `SpearAttack.SpearStatus`.
enum class SpearStatus : std::uint8_t { Approach, Charging, Retreat };

using Pos = std::array<std::int32_t, 3>;

struct PiglinState {
    bool brute = false;
    bool cannotHunt = false;
    std::set<std::string> barterItems;
    std::set<std::string> interestItems;
    std::array<std::optional<server::world::loot::ItemStack>, kInventorySize> inventory;
    ai::brainKit::Memory<wire::common::Uuid> angryAt;
    ai::brainKit::Memory<bool> universalAnger;
    ai::brainKit::Memory<std::int32_t> avoidTarget;
    ai::brainKit::Memory<bool> huntedRecently;
    ai::brainKit::Memory<bool> admiring;
    ai::brainKit::Memory<bool> admiringDisabled;
    ai::brainKit::Memory<bool> walkToItemDisabled;
    std::optional<std::int32_t> timeTryingToReachItem;
    ai::brainKit::Memory<bool> ateRecently;
    ai::brainKit::Memory<Pos> celebrateLocation;
    bool dancing = false;
    ai::brainKit::Memory<std::int32_t> rideTarget;
    std::optional<Pos> home;
    Activity activity = Activity::Idle;
    ai::brainKit::Motion motion;
    ai::brainKit::Look look;
    ai::doorInteraction::State doors;
    std::int64_t attackCoolingUntil = 0;
    std::int32_t idleLookTicks = 0;
    std::int32_t idleMoveTicks = 0;
    Crossbow crossbow = Crossbow::Uncharged;
    std::int32_t crossbowTicks = 0;
    std::int32_t crossbowDelay = 0;
    bool chargingCrossbow = false;
    std::int32_t rideTicker = 0;
    SpearStatus spear = SpearStatus::Approach;
    std::int32_t spearEngageTime = 0;
    std::optional<Pos> spearChargePos;
    std::int32_t spearFleeingTime = 0;
    std::optional<Pos> spearFleeingPos;
    std::optional<std::int32_t> wantedItem;
    bool memoriesSeeded = false;
    std::int64_t lastTick = 0;
};

[[nodiscard]] bool isPiglin(const wire::common::Identifier& type);
[[nodiscard]] bool isBrute(const wire::common::Identifier& type);

[[nodiscard]] PiglinState& stateOf(ServerEntity& piglin);

void write(const ServerEntity& piglin, wire::nbt::NbtCompound& tag);

/// parity: `SimpleContainer.addItem`: what did not fit.
[[nodiscard]] std::optional<server::world::loot::ItemStack> addToInventory(PiglinState& state,
                                                                           server::world::loot::ItemStack stack);

[[nodiscard]] bool canAddToInventory(const PiglinState& state, const server::world::loot::ItemStack& stack);

} // namespace piglins

} // namespace basalt::server::runtime::entity
