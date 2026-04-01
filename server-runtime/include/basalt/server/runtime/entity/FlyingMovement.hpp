#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// How something that flies moves: its own move control, then its own travel.
///
/// parity: `FlyingMoveControl` over `travelInAir` (wither, parrot, bee, allay), `GhastMoveControl`
/// over `travelFlying`, `VexMoveControl`, `PhantomMoveControl` and the bat's `customServerAiStep`.
namespace flyingMovement {

enum class Control : std::uint8_t {
    /// Nothing steers it: the drift of `travelFlying` with no input.
    Drift,
    /// parity: `FlyingMoveControl`.
    Flying,
    /// parity: `Ghast.GhastMoveControl`, not careful.
    Ghast,
    /// parity: `Vex.VexMoveControl`, with `noPhysics`.
    Vex,
    /// parity: `Phantom.PhantomMoveControl`.
    Phantom,
    /// parity: `Bat.customServerAiStep`, which steers it without any control.
    Bat,
};

[[nodiscard]] Control controlOf(const wire::common::Identifier& type);

/// parity: the fliers whose navigation is `FlyingPathNavigation`, so their paths are flown.
[[nodiscard]] bool usesFlyingPath(const wire::common::Identifier& type);

/// parity: `GhastMoveControl.canReach`, not careful: no block's shape along the way is in the box's
/// path, those it starts in aside.
[[nodiscard]] bool ghastCanReach(world::WorldState& world, const ServerEntity& mob, double dx, double dy, double dz);

/// One tick of the flier's move control and travel. `target` is where its attack target stands,
/// which the vex turns to face.
void tick(world::WorldState& world, ServerEntity& entity, random::RandomSource& random,
          const std::optional<std::array<double, 3>>& target = std::nullopt);

} // namespace flyingMovement

/// The mobs that fly rather than walk, which is what decides how they are moved every tick.
namespace flyers {

[[nodiscard]] bool flies(const wire::common::Identifier& type);

} // namespace flyers

} // namespace basalt::server::runtime::entity
