#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `ServerExplosion.calculateExplodedPositions`.
namespace explosions {

struct Broken {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;
    std::optional<wire::nbt::NbtCompound> hive;
    /// `JukeboxBlockEntity.setRemoved` sends the stop event even when no player took the disc.
    bool songWasPlaying = false;

    friend bool operator==(const Broken&, const Broken&) = default;
};

/// parity: `ExplosionDamageCalculator`'s hooks.
using Spares =
    std::function<bool(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

/// parity: `ExplosionDamageCalculator`.
using Resistance = std::function<float(std::int32_t stateId, float resistance)>;

/// parity: `MinecartTNT.getBlockExplosionResistance`.
[[nodiscard]] bool sparesRails(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

/// The blocks are already gone from the world; their drops are the caller's business, as in vanilla.
[[nodiscard]] std::vector<Broken> explode(WorldState& world, std::int32_t minY, std::int32_t maxY, double x, double y,
                                          double z, float power, random::RandomSource& random,
                                          const Spares& spares = {}, const Resistance& resistance = {});

/// parity: `calculateExplodedPositions`.
[[nodiscard]] std::vector<Broken> reaches(WorldState& world, std::int32_t minY, std::int32_t maxY, double x, double y,
                                          double z, float power, random::RandomSource& random,
                                          const Spares& stops = {});

/// parity: `ServerExplosion.hurtEntities` without the exposure ray cast.
[[nodiscard]] float damageAt(double distance, float power, float radius);

} // namespace explosions

} // namespace basalt::server::runtime::world
