#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// A neutral mob's anger: until when it lasts and at whom.
///
/// parity: `NeutralMob`, for the mobs that are not bees (the bee keeps its own alongside the rest of
/// its state).
namespace neutralAnger {

/// parity: `TimeUtil.rangeOfSeconds(20, 39)`, every neutral mob's `PERSISTENT_ANGER_TIME`.
inline constexpr std::int32_t kAngerMinTicks = 20 * 20;
inline constexpr std::int32_t kAngerMaxTicks = 39 * 20;

struct Anger {
    std::int64_t endTime = -1;
    std::optional<wire::common::Uuid> target;
    /// parity: the old `AngerTime`, ticks from load, settled on the first turn.
    std::optional<std::int32_t> legacyTime;
    /// parity: `ZombifiedPiglin.playFirstAngerSoundIn` and `ticksUntilNextAlert`.
    std::int32_t firstSoundIn = 0;
    std::int32_t nextAlertIn = 0;
    bool hadTarget = false;
    /// parity: `Enderman.DATA_STARED_AT`, until the target is dropped.
    bool staredAt = false;
};

/// The anger, made on first use from what it saved.
[[nodiscard]] Anger& stateOf(ServerEntity& mob);

/// parity: `addPersistentAngerSaveData`.
void write(const ServerEntity& mob, wire::nbt::NbtCompound& tag);

[[nodiscard]] bool isAngry(const Anger& anger, std::int64_t gameTime);

/// parity: `startPersistentAngerTimer` with the mob's own range in ticks.
void start(Anger& anger, std::int64_t gameTime, random::RandomSource& random, std::int32_t minTicks,
           std::int32_t maxTicks);

/// parity: `stopBeingAngry`, which also forgets the target and who hurt it.
void stop(ServerEntity& mob, Anger& anger);

/// Whether it is a `NeutralMob`: bee, wolf, iron golem, polar bear, enderman or zombified piglin.
[[nodiscard]] bool isNeutral(const wire::common::Identifier& type);

/// Where a neutral mob keeps its anger: a bee in its own state, the rest in `Anger`.
struct Access {
    std::int64_t* endTime = nullptr;
    std::optional<wire::common::Uuid>* target = nullptr;
};

/// Empty for a mob that is not neutral, or a bee whose state is not made yet.
[[nodiscard]] Access accessOf(ServerEntity& mob);

/// parity: `NeutralMob.isAngryAtAllPlayers`.
[[nodiscard]] bool isAngryAtAllPlayers(ServerEntity& mob, std::int64_t gameTime, bool universalAnger);

/// parity: `NeutralMob.isAngryAt` for a player; `valid` is `canAttack` and `isValidPlayerTarget`.
[[nodiscard]] bool isAngryAtPlayer(ServerEntity& mob, const wire::common::Uuid& player, bool valid,
                                   std::int64_t gameTime, bool universalAnger);

/// parity: `NeutralMob.stopBeingAngry` for any neutral mob.
void stopBeingAngry(ServerEntity& mob);

/// parity: `NeutralMob.forgetCurrentTargetAndRefreshUniversalAnger`.
void refreshUniversalAnger(ServerEntity& mob, std::int64_t gameTime, random::RandomSource& random);

/// parity: `NeutralMob.playerDied`, once `forgive_dead_players` has been read.
void playerDied(ServerEntity& mob, const wire::common::Uuid& player);

} // namespace neutralAnger

} // namespace basalt::server::runtime::entity
