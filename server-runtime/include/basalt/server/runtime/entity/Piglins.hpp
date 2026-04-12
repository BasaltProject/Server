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

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The piglin and the piglin brute: the gold they admire and barter, the hoglins they hunt, what
/// they fear and whom they are angry at.
///
/// parity: `Piglin`, `PiglinBrute`, `AbstractPiglin`, `PiglinAi` and `PiglinBruteAi`.
namespace piglins {

/// parity: `AbstractPiglin.DATA_IMMUNE_TO_ZOMBIFICATION` then `Piglin`'s baby, crossbow and dance.
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
/// parity: `TimeUtil.rangeOfSeconds` for hunts, retreats and the zombified.
inline constexpr std::array<std::int32_t, 2> kTimeBetweenHunts{30 * 20, 120 * 20};
inline constexpr std::array<std::int32_t, 2> kRetreatDuration{5 * 20, 20 * 20};
inline constexpr std::array<std::int32_t, 2> kAvoidZombified{5 * 20, 7 * 20};

/// parity: `Activity`, the ones a piglin moves between.
enum class Activity : std::uint8_t { Idle, AdmireItem, Fight, Avoid, Celebrate, Ride };

/// parity: `CrossbowAttack.CrossbowState`.
enum class Crossbow : std::uint8_t { Uncharged, Charging, Charged, ReadyToAttack };

using Pos = std::array<std::int32_t, 3>;

/// What a piglin keeps beyond the entity itself, its brain's memories included.
struct PiglinState {
    bool brute = false;
    bool cannotHunt = false;
    std::array<std::optional<server::world::loot::ItemStack>, kInventorySize> inventory;
    ai::brainKit::Memory<wire::common::Uuid> angryAt;
    /// parity: `MemoryModuleType.UNIVERSAL_ANGER`, six hundred ticks of anger at every player.
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
    /// parity: the `DoNothing` a `RunOne` is waiting out, one for looking and one for moving.
    std::int32_t idleLookTicks = 0;
    std::int32_t idleMoveTicks = 0;
    Crossbow crossbow = Crossbow::Uncharged;
    std::int32_t crossbowTicks = 0;
    std::int32_t crossbowDelay = 0;
    bool chargingCrossbow = false;
    /// parity: `SetEntityLookTargetSometimes.Ticker` for a baby's ride.
    std::int32_t rideTicker = 0;
    /// The nearest wanted item, as `NearestItemSensor` left it.
    std::optional<std::int32_t> wantedItem;
    bool memoriesSeeded = false;
    /// The game time of its last turn, which a save counts the memories' time left from.
    std::int64_t lastTick = 0;
};

[[nodiscard]] bool isPiglin(const wire::common::Identifier& type);
[[nodiscard]] bool isBrute(const wire::common::Identifier& type);

/// The state, made on first use from what it saved.
[[nodiscard]] PiglinState& stateOf(ServerEntity& piglin);

/// parity: `addAdditionalSaveData`: the inventory, `CannotHunt` and the brain's anger and hunt.
void write(const ServerEntity& piglin, wire::nbt::NbtCompound& tag);

/// parity: `SimpleContainer.addItem`: what did not fit.
[[nodiscard]] std::optional<server::world::loot::ItemStack> addToInventory(PiglinState& state,
                                                                           server::world::loot::ItemStack stack);

/// parity: `SimpleContainer.canAddItem`.
[[nodiscard]] bool canAddToInventory(const PiglinState& state, const server::world::loot::ItemStack& stack);

} // namespace piglins

} // namespace basalt::server::runtime::entity
