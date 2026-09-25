#pragma once

#include <basalt/server/random/RandomSource.hpp>
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

struct Anger {
    std::int64_t endTime = -1;
    std::optional<wire::common::Uuid> target;
    /// parity: the old `AngerTime`, ticks from load, settled on the first turn.
    std::optional<std::int32_t> legacyTime;
    /// parity: `ZombifiedPiglin.playFirstAngerSoundIn` and `ticksUntilNextAlert`.
    std::int32_t firstSoundIn = 0;
    std::int32_t nextAlertIn = 0;
    bool hadTarget = false;
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

} // namespace neutralAnger

} // namespace basalt::server::runtime::entity
