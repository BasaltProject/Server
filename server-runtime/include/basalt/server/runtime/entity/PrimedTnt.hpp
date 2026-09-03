#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {
class WorldState;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The block of TNT once it is lit: an entity that falls, slides and goes off when its fuse runs out.
///
/// parity: `PrimedTnt`. It is not the block exploding where it stood: the entity keeps 0.7 of its
/// sideways speed on landing and half its fall, so TNT pushed by a piston or dropped off a ledge goes
/// off somewhere else. That difference is what a cannon is built on.
namespace primedTnt {

[[nodiscard]] const wire::common::Identifier& type();

/// parity: `PrimedTnt.DEFAULT_FUSE_TIME`, four seconds.
inline constexpr std::int32_t kDefaultFuse = 80;

/// parity: `PrimedTnt.DEFAULT_EXPLOSION_POWER`.
inline constexpr float kExplosionPower = 4.0F;

/// parity: the 1/16 of a block above its feet the explosion is centred on (`getY(0.0625)`).
inline constexpr double kExplosionOffset = 0.0625;

/// Lights the TNT that stood at these block coordinates, as `TntBlock.explode` spawns it.
ServerEntity& prime(world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t entityId,
                    std::int32_t protocolType, std::int32_t fuse = kDefaultFuse);

/// One tick: it moves, then the fuse burns down. True when the fuse ran out, which is when the
/// caller explodes it.
///
/// parity: `PrimedTnt.tick`, in its order: the move happens before the fuse is counted, so TNT lit
/// at the top of a fall goes off where it lands and not where it started.
bool tick(world::WorldState& world, ServerEntity& entity);

} // namespace primedTnt

} // namespace basalt::server::runtime::entity
