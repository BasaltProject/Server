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

/// Which way a line of redstone dust runs.
///
/// parity: `RedStoneWireBlock.getConnectionState`. A dust reaches towards whatever it can carry a
/// signal to, climbs one block when the way up is clear, and when it finds nothing on one axis it
/// straightens out along the other rather than sitting there as a dot.
namespace redstoneWire {

[[nodiscard]] const wire::common::Identifier& block();

[[nodiscard]] bool isWire(std::int32_t stateId);

/// parity: `getConnectionState`, including the "a dot stays a dot" clause.
[[nodiscard]] std::map<std::string, std::string> connectionsFor(WorldState& world, std::int32_t x, std::int32_t y,
                                                                std::int32_t z, std::int32_t stateId);

/// The shape the dust here should have, written if it moved, or nothing when it was already right.
///
/// parity: `RedStoneWireBlock.updateShape`. `towards` decides how much is redrawn: from below
/// nothing, from above all four sides, from a side only that one unless its connection moved or the
/// dust is a cross. No direction redraws all four, which is what a dust that just landed does.
[[nodiscard]] std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId,
                                                  std::optional<registry::BlockFace> towards = std::nullopt);

/// What a right click turns this dust into: a cross becomes a dot and a dot becomes a cross, and a
/// line is left alone. parity: `RedStoneWireBlock.useWithoutItem` only answers for `isCross` or
/// `isDot`, keeps the power and redraws with `getConnectionState`.
[[nodiscard]] std::optional<std::int32_t> toggleShape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                      std::int32_t stateId);

/// The shape a dust has as it lands, written if it moved. parity:
/// `RedStoneWireBlock.getStateForPlacement` draws from the cross rather than from the state it
/// starts with, so a dust put down on its own is a cross and never a dot.
[[nodiscard]] std::optional<std::int32_t> shapeOnPlacement(WorldState& world, std::int32_t x, std::int32_t y,
                                                           std::int32_t z, std::int32_t stateId);

/// A dust a change here has to reach across a step, and the side it hears the news from.
struct Step {
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
    registry::BlockFace towards;
};

/// The dusts a change here has to reach across a step, in the order vanilla walks them. parity:
/// `RedStoneWireBlock.updateIndirectNeighbourShapes`: the dust below and above a connected
/// non-dust square. Without it a line running up a staircase is drawn as if the steps were not there.
[[nodiscard]] std::vector<Step> acrossSteps(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The same reach read off a state that is no longer there. `Level.setBlock` runs the walk on the
/// state that left as well as on the one that arrived, so a broken line still tells the dust it used
/// to reach across the step.
[[nodiscard]] std::vector<Step> acrossSteps(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                            std::int32_t stateId);

} // namespace redstoneWire

} // namespace basalt::server::runtime::world
