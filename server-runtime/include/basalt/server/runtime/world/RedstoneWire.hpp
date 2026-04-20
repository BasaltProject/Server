#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::world {

/// parity: `RedStoneWireBlock.getConnectionState`.
namespace redstoneWire {

[[nodiscard]] const wire::common::Identifier& block();

[[nodiscard]] bool isWire(std::int32_t stateId);

/// parity: `getConnectionState`, including the "a dot stays a dot" clause.
[[nodiscard]] std::map<std::string, std::string> connectionsFor(const WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z, std::int32_t stateId);

/// parity: `RedStoneWireBlock.updateShape`.
[[nodiscard]] std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId,
                                                  std::optional<registry::BlockFace> towards = std::nullopt);

/// parity: `useWithoutItem` answers only for `isCross` or `isDot`.
[[nodiscard]] std::optional<std::int32_t> toggleShape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                      std::int32_t stateId);

/// parity: `getStateForPlacement` draws from the cross.
[[nodiscard]] std::optional<std::int32_t> shapeOnPlacement(WorldState& world, std::int32_t x, std::int32_t y,
                                                           std::int32_t z, std::int32_t stateId);

struct Step {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
    registry::BlockFace towards;
};

/// parity: `updateIndirectNeighbourShapes`.
[[nodiscard]] std::vector<Step> acrossSteps(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::vector<Step> acrossSteps(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::int32_t stateId);

} // namespace redstoneWire

} // namespace basalt::server::runtime::world
