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

/// parity: `Painting.create`.
namespace paintings {

[[nodiscard]] std::optional<registry::vanillaRegistryData::Painting> fitting(const WorldState& world, std::int32_t x,
                                                                             std::int32_t y, std::int32_t z,
                                                                             server::registry::BlockFace facing,
                                                                             random::RandomSource& random);

/// parity: `HangingEntity.survives`.
[[nodiscard]] bool survives(const WorldState& world, const entity::ServerEntity& painting);

/// parity: `Painting.calculateBoundingBox`.
[[nodiscard]] std::array<double, 3> hangingAt(std::int32_t x, std::int32_t y, std::int32_t z,
                                              server::registry::BlockFace facing, std::int32_t width,
                                              std::int32_t height);

} // namespace paintings

} // namespace basalt::server::runtime::world
