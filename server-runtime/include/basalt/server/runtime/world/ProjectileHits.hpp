#pragma once

#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>

namespace basalt::server::runtime::world {

/// What a block does when something hits it in flight.
///
/// parity: the ten `onProjectileHit` overrides in the jar. The one that matters to redstone is the
/// target block, whose signal depends on *where* it was hit: dead centre is fifteen, the rim is one.
namespace projectileHits {

/// parity: `TargetBlock.updateRedstoneOutput`, twenty ticks for an arrow and eight for the rest.
inline constexpr std::int32_t kArrowHoldTicks = 20;
inline constexpr std::int32_t kThrownHoldTicks = 8;

/// Answers what changed, if anything. `byArrow` decides how long a target holds its signal.
/// parity: `TntBlock.onProjectileHit` asks the projectile whether it is on fire and nothing else, so
/// a plain arrow sticks in the block and leaves it standing. `gameTime` is the world's clock, which
/// a dripleaf tipped by an arrow books its return on.
/// `tntExplodes` is the game rule: with it off `TntBlock.prime` answers false and the block stays.
///
/// `destroyed` is told the state of a block the hit **broke**, which is what rolls its loot table:
/// `DecoratedPotBlock.onProjectileHit` ends in `destroyBlock(pos, true, projectile)` and a pot that
/// nobody decorated leaves its four bricks on the floor.
using OnDestroyed = std::function<void(std::int32_t stateId, std::int32_t x, std::int32_t y, std::int32_t z)>;

[[nodiscard]] std::optional<BlockChange> onHit(WorldState& world, std::int64_t gameTime, const entity::BlockHit& hit,
                                               bool byArrow, bool burning, bool tntExplodes,
                                               const OnDestroyed& destroyed = {});

/// parity: `TargetBlock.getRedstoneStrength`: the distance from the middle of the face that was
/// hit, over the half block that face is wide, is what decides the signal.
[[nodiscard]] std::int32_t signalFor(const entity::BlockHit& hit);

/// The other half of a target: the scheduled tick that lets its signal go.
[[nodiscard]] std::optional<BlockChange> releaseTarget(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z);

} // namespace projectileHits

} // namespace basalt::server::runtime::world
