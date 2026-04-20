#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// parity: `FlyingMoveControl`.
namespace flyingMovement {

enum class Control : std::uint8_t {
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

/// parity: the fliers whose navigation is `FlyingPathNavigation`.
[[nodiscard]] bool usesFlyingPath(const wire::common::Identifier& type);

/// parity: `GhastMoveControl.canReach`.
[[nodiscard]] bool ghastCanReach(world::WorldState& world, const ServerEntity& mob, double dx, double dy, double dz);

void tick(world::WorldState& world, ServerEntity& entity, random::RandomSource& random,
          const std::optional<std::array<double, 3>>& target = std::nullopt);

} // namespace flyingMovement

namespace flyers {

[[nodiscard]] bool flies(const wire::common::Identifier& type);

} // namespace flyers

} // namespace basalt::server::runtime::entity
