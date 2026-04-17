#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::runtime::world {

/// An explosion: which blocks it takes out, and how hard it hits what is standing near. parity:
/// `ServerExplosion.calculateExplodedPositions`, ray for ray: the shell of a 16^3 cube of
/// directions, each stepped 0.3 blocks while its power lasts, every block spending
/// `(resistance + 0.3) * 0.3` of it.
namespace explosions {

/// parity: `BedBlock`'s own five, the power of a bed that will not let you sleep.
inline constexpr float kBedPower = 5.0F;

/// A block an explosion took out, so the caller can drop it and tell the clients.
struct Broken {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;
    /// A beehive's block entity as it was, which `BeehiveBlock.getDrops` empties.
    std::optional<wire::nbt::NbtCompound> hive;
    /// A jukebox whose song the blast cut short, which `JukeboxBlockEntity.setRemoved` tells the
    /// world about with the stop event even when no player took the disc out.
    bool songWasPlaying = false;

    friend bool operator==(const Broken&, const Broken&) = default;
};

/// What the source of a blast lets off: a block this answers true for spends nothing of the ray and
/// is never broken. parity: `ExplosionDamageCalculator`'s two hooks, the only say a source entity
/// has over what its own explosion takes out.
using Spares =
    std::function<bool(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

/// What a source makes of a block's blast resistance. parity: the other half of
/// `ExplosionDamageCalculator`: a blue wither skull answers at most `0.8` for anything it may break.
using Resistance = std::function<float(std::int32_t stateId, float resistance)>;

/// parity: `MinecartTNT.getBlockExplosionResistance` and `MinecartTNT.shouldBlockExplode`: a primed
/// cart spares the rails it was running on and whatever is holding them up.
[[nodiscard]] bool sparesRails(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId);

/// Blows a hole and answers what it took out. The blocks are already gone from the world; what they
/// drop is the caller's business, as it is in vanilla.
[[nodiscard]] std::vector<Broken> explode(WorldState& world, std::int32_t minY, std::int32_t maxY, double x, double y,
                                          double z, float power, random::RandomSource& random,
                                          const Spares& spares = {}, const Resistance& resistance = {});

/// Which squares a blast reaches without taking any of them out. parity:
/// `calculateExplodedPositions` on its own, which is all a `TRIGGER_BLOCK` explosion uses: the wind
/// burst breaks nothing. A spared block costs the ray `3600000`, which stops it dead; everything
/// else costs only the step.
[[nodiscard]] std::vector<Broken> reaches(WorldState& world, std::int32_t minY, std::int32_t maxY, double x, double y,
                                          double z, float power, random::RandomSource& random,
                                          const Spares& stops = {});

/// How much damage this explosion does to something `distance` blocks away.
///
/// parity: `ServerExplosion.hurtEntities`' own arithmetic, without the exposure ray cast: the seen
/// fraction is one here, which is what an entity in the open gets anyway.
[[nodiscard]] float damageAt(double distance, float power, float radius);

} // namespace explosions

} // namespace basalt::server::runtime::world
