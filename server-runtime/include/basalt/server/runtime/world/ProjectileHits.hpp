#pragma once

#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
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

/// Answers what changed, if anything. `byArrow` decides how long a target holds its signal, which
/// is the only thing the kind of projectile changes here.
[[nodiscard]] std::optional<BlockChange> onHit(WorldState& world, const entity::BlockHit& hit, bool byArrow);

/// parity: `TargetBlock.getRedstoneStrength`: the distance from the middle of the face that was
/// hit, over the half block that face is wide, is what decides the signal.
[[nodiscard]] std::int32_t signalFor(const entity::BlockHit& hit);

/// The other half of a target: the scheduled tick that lets its signal go.
[[nodiscard]] std::optional<BlockChange> releaseTarget(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z);

} // namespace projectileHits

} // namespace basalt::server::runtime::world
