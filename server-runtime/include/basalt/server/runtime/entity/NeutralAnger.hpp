#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/common/Uuid.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `NeutralMob`.
namespace neutralAnger {

inline constexpr std::int32_t kAngerMinTicks = 20 * 20;
inline constexpr std::int32_t kAngerMaxTicks = 39 * 20;

struct Anger {
    std::int64_t endTime = -1;
    std::optional<wire::common::Uuid> target;
    std::optional<std::int32_t> legacyTime;
    std::int32_t firstSoundIn = 0;
    std::int32_t nextAlertIn = 0;
    bool hadTarget = false;
    /// parity: `Enderman.DATA_STARED_AT`, until the target is dropped.
    bool staredAt = false;
};

[[nodiscard]] Anger& stateOf(ServerEntity& mob);

void write(const ServerEntity& mob, wire::nbt::NbtCompound& tag);

[[nodiscard]] bool isAngry(const Anger& anger, std::int64_t gameTime);

void start(Anger& anger, std::int64_t gameTime, random::RandomSource& random, std::int32_t minTicks,
           std::int32_t maxTicks);

void stop(ServerEntity& mob, Anger& anger);

[[nodiscard]] bool isNeutral(const wire::common::Identifier& type);

struct Access {
    std::int64_t* endTime = nullptr;
    std::optional<wire::common::Uuid>* target = nullptr;
};

[[nodiscard]] Access accessOf(ServerEntity& mob);

[[nodiscard]] bool isAngryAtAllPlayers(ServerEntity& mob, std::int64_t gameTime, bool universalAnger);

[[nodiscard]] bool isAngryAtPlayer(ServerEntity& mob, const wire::common::Uuid& player, bool valid,
                                   std::int64_t gameTime, bool universalAnger);

void stopBeingAngry(ServerEntity& mob);

void refreshUniversalAnger(ServerEntity& mob, std::int64_t gameTime, random::RandomSource& random);

void playerDied(ServerEntity& mob, const wire::common::Uuid& player);

} // namespace neutralAnger

} // namespace basalt::server::runtime::entity
