#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
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

/// parity: `ExplosionDamageCalculator.getEntityDamageAmount`; `doubleRadius` is twice the blast radius.
[[nodiscard]] float damageAt(double distance, float doubleRadius, float exposure = 1.0F);

struct Box {
    double minX = 0.0;
    double minY = 0.0;
    double minZ = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    double maxZ = 0.0;
};

/// parity: `ServerExplosion.getSeenPercent`.
[[nodiscard]] float seenPercent(WorldState& world, double centerX, double centerY, double centerZ, const Box& box);

/// What `seenPercent` found for each entity before the blast broke any block, which vanilla also measures first.
void clearExposures();

void recordExposure(std::int32_t entityId, float exposure);

[[nodiscard]] float exposureOf(std::int32_t entityId);

/// parity: `ServerExplosion.hurtEntities`: the push an entity at `entity` (its eye or, for primed TNT, its position).
[[nodiscard]] std::array<double, 3> knockbackOf(double centerX, double centerY, double centerZ, double originX,
                                                double originY, double originZ, double distance, float exposure,
                                                double resistance);

} // namespace explosions

} // namespace basalt::server::runtime::world
