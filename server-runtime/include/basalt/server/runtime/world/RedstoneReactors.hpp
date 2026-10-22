#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `neighborChanged`.
namespace redstoneReactors {

inline constexpr std::int32_t kLampOffDelay = 4;

[[nodiscard]] const wire::common::Identifier& lamp();

/// parity: `neighborChanged` on the one block at (x, y, z).
[[nodiscard]] std::vector<BlockChange> reactAt(WorldState& world, std::string_view worldName, std::int32_t x,
                                               std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                               random::RandomSource& random, bool tntExplodes,
                                               std::optional<std::int32_t> told = std::nullopt);

/// parity: `updateNeighborsAt`.
[[nodiscard]] std::vector<BlockChange> update(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                              random::RandomSource& random, bool tntExplodes, bool itself = true,
                                              std::optional<std::int32_t> told = std::nullopt);

/// parity: `onPlace`.
[[nodiscard]] std::vector<BlockChange> placed(WorldState& world, std::string_view worldName, std::int32_t x,
                                              std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                              random::RandomSource& random, bool tntExplodes,
                                              std::optional<std::int32_t> removed = std::nullopt);

/// parity: `DiodeBlock.updateNeighborsInFront`.
[[nodiscard]] std::vector<BlockChange> updateInFront(WorldState& world, std::string_view worldName, std::int32_t x,
                                                     std::int32_t y, std::int32_t z, server::registry::BlockFace back,
                                                     std::int64_t gameTime, random::RandomSource& random,
                                                     bool tntExplodes);

/// parity: `updatePowerStrength`.
[[nodiscard]] std::vector<BlockChange> updateAroundWire(WorldState& world, std::string_view worldName, std::int32_t x,
                                                        std::int32_t y, std::int32_t z, std::int64_t gameTime,
                                                        random::RandomSource& random, bool tntExplodes);

/// Only goes dark if the signal is still gone.
[[nodiscard]] std::optional<BlockChange> lampTick(WorldState& world, std::string_view worldName, std::int32_t x,
                                                  std::int32_t y, std::int32_t z);

} // namespace redstoneReactors

} // namespace basalt::server::runtime::world
