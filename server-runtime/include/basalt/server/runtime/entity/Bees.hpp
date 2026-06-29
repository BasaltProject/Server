#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `Bee` and the `NeutralMob` half of it.
namespace bees {

/// parity: `DATA_FLAGS_ID` and `DATA_ANGER_END_TIME`, after `AgeableMob`'s baby flag.
inline constexpr std::uint8_t kFlagsDataIndex = 17;
inline constexpr std::uint8_t kAngerEndDataIndex = 18;
inline constexpr std::int8_t kFlagRoll = 2;
inline constexpr std::int8_t kFlagHasStung = 4;
inline constexpr std::int8_t kFlagHasNectar = 8;

inline constexpr std::int32_t kStingDeathCountdown = 1200;
inline constexpr std::int32_t kTicksBeforeKnownFlower = 600;
inline constexpr std::int32_t kTicksWithoutNectarBeforeHome = 3600;
inline constexpr std::int32_t kMaxCropsGrowable = 10;
inline constexpr std::int32_t kTooFarDistance = 48;
inline constexpr std::int32_t kHiveCloseEnough = 2;
inline constexpr std::int32_t kPathfindToHiveWhenCloserThan = 16;
inline constexpr std::int32_t kHiveSearchDistance = 20;
inline constexpr std::int32_t kHiveCooldown = 200;
inline constexpr std::int32_t kFlowerCooldown = 200;
inline constexpr std::int32_t kFlowerRetryMin = 20;
inline constexpr std::int32_t kFlowerRetryMax = 60;
/// parity: `PERSISTENT_ANGER_TIME`, twenty to thirty-nine seconds.
inline constexpr std::int32_t kAngerMinTicks = 20 * 20;
inline constexpr std::int32_t kAngerMaxTicks = 39 * 20;
/// parity: `setRequiredPathLength(48)`.
inline constexpr float kPathLength = 48.0F;

using Pos = std::array<std::int32_t, 3>;

struct BeeState {
    std::optional<Pos> hive;
    std::optional<Pos> flower;
    bool hasNectar = false;
    bool hasStung = false;
    bool rolling = false;
    /// Set by a plugin: `-1` follows the bee's anger, `0` never rolls, `1` always does.
    std::int8_t rollingOverride = -1;
    std::int32_t ticksWithoutNectar = 0;
    std::int32_t stayOutOfHive = 0;
    std::int32_t cropsGrown = 0;
    std::int32_t hiveCooldown = 0;
    std::int32_t flowerCooldown = 0;
    std::int32_t timeSinceSting = 0;
    std::int32_t underWaterTicks = 0;
    /// parity: `NeutralMob`'s end time and `persistentAngerTarget`.
    std::int64_t angerEndTime = -1;
    std::optional<wire::common::Uuid> angryAt;
    /// parity: the old `AngerTime`.
    std::optional<std::int32_t> legacyAngerTime;
    /// parity: `BeePollinateGoal.pollinating`, which the navigation and the hive both read.
    bool pollinating = false;
    /// parity: `BeeGoToHiveGoal.blacklistedTargets`.
    std::vector<Pos> blacklistedHives;
};

[[nodiscard]] bool isBee(const wire::common::Identifier& type);

[[nodiscard]] BeeState& stateOf(ServerEntity& bee, random::RandomSource& random);

/// parity: `addAdditionalSaveData` and `addPersistentAngerSaveData`.
void write(const ServerEntity& bee, wire::nbt::NbtCompound& tag);

/// Takes the saved fields of the bee's source data back into its live state, after a plugin changed them.
void reload(ServerEntity& bee);

[[nodiscard]] std::int8_t flagsOf(const BeeState& state);

/// parity: `NeutralMob.isAngry`.
[[nodiscard]] bool isAngry(const BeeState& state, std::int64_t gameTime);

/// parity: `startPersistentAngerTimer`.
void startAngerTimer(BeeState& state, std::int64_t gameTime, random::RandomSource& random);

/// parity: `NeutralMob.stopBeingAngry`, which also forgets the target and who hurt it.
void stopBeingAngry(ServerEntity& bee, BeeState& state);

/// parity: `setHasNectar`, which resets the nectarless count on a yes.
void setHasNectar(BeeState& state, bool hasNectar);

/// parity: `dropHive` and `dropFlower`.
void dropHive(BeeState& state);
void dropFlower(BeeState& state, random::RandomSource& random);

/// parity: `Vec3i.closerThan` from the bee's block.
[[nodiscard]] bool closerThan(const ServerEntity& bee, const Pos& target, double distance);

/// parity: `BeehiveBlockEntity.IGNORED_BEE_TAGS`.
void stripHiveTags(wire::nbt::NbtCompound& tag);

} // namespace bees

} // namespace basalt::server::runtime::entity
