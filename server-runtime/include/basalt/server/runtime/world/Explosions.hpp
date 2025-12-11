#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::world {

/// An explosion: which blocks it takes out, and how hard it hits what is standing near.
///
/// parity: `ServerExplosion.calculateExplodedPositions`, ray for ray. Sixteen cubed directions, only
/// the ones on the shell of that cube, each stepped 0.3 blocks at a time while its power lasts, and
/// every block it passes spending `(resistance + 0.3) * 0.3` of it.
///
/// A bed in the Nether is what asks for one today; TNT and creepers will ask for the same routine.
namespace explosions {

/// parity: `BedBlock`'s own five, the power of a bed that will not let you sleep.
inline constexpr float kBedPower = 5.0F;

/// A block an explosion took out, so the caller can drop it and tell the clients.
struct Broken {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
    std::int32_t stateId = 0;

    friend bool operator==(const Broken&, const Broken&) = default;
};

/// Blows a hole and answers what it took out. The blocks are already gone from the world; what they
/// drop is the caller's business, as it is in vanilla.
[[nodiscard]] std::vector<Broken> explode(WorldState& world, std::int32_t minY, std::int32_t maxY, double x, double y,
                                          double z, float power, random::RandomSource& random);

/// How much damage this explosion does to something `distance` blocks away.
///
/// parity: `ServerExplosion.hurtEntities`' own arithmetic, without the exposure ray cast: the seen
/// fraction is one here, which is what an entity in the open gets anyway.
[[nodiscard]] float damageAt(double distance, float power, float radius);

} // namespace explosions

} // namespace basalt::server::runtime::world
