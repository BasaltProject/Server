#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/registry/VanillaRegistryData.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

/// Hanging a painting: which art fits the wall and where its middle ends up.
///
/// parity: `Painting.create` plus `HangingEntity.survives`. The variant is not the player's choice:
/// vanilla keeps every placeable one whose whole back is against solid blocks, throws away all but
/// the largest by area, and picks one of those at random.
namespace paintings {

/// The art a painting hung on that face would carry, or nothing when the wall cannot hold any.
[[nodiscard]] std::optional<registry::vanillaRegistryData::Painting> fitting(const WorldState& world, std::int32_t x,
                                                                             std::int32_t y, std::int32_t z,
                                                                             server::registry::BlockFace facing,
                                                                             random::RandomSource& random);

/// parity: `HangingEntity.survives` for a painting already on the wall. Another hanging entity
/// sharing its squares is not asked about here.
[[nodiscard]] bool survives(const WorldState& world, const entity::ServerEntity& painting);

/// parity: `Painting.calculateBoundingBox`: the square's centre pushed back against the wall, then
/// half a block along the left and up for each even side.
[[nodiscard]] std::array<double, 3> hangingAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                              server::registry::BlockFace facing, std::int32_t width,
                                              std::int32_t height);

} // namespace paintings

} // namespace basalt::server::runtime::world
