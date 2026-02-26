#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::server::runtime::world {

/// The blocks whose `updateShape` changes what they are rather than whether they stay.
///
/// parity: of the 104 classes that override `updateShape`, 87 only ask whether they can survive or
/// schedule a fluid tick, and both of those already happen elsewhere. These are the rest: a gate
/// that sinks into a wall, a chest that pairs up, a bed that loses its other half.
///
/// Fences, panes, walls, stairs and leaves are in `BlockConnections` and `NeighbourUpdates`, and
/// redstone wire and repeaters in `Redstone`.
namespace reshapes {

/// The chests that pair up, which is every wood and copper variant.
///
/// parity: the same list `ChestBlock` covers. It lives here until the container code that also needs
/// it is ported, and then that owns it.
[[nodiscard]] std::span<const std::string_view> chestBlocks();

/// Whether water reaches this square from any side the jar counts.
///
/// parity: `ConcretePowderBlock.touchesLiquid`, quirk included: the block **underneath** never counts,
/// because the first turn of the loop asks the powder itself whether to look down.
[[nodiscard]] bool touchesWater(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

/// The concrete this powder sets into, or absent for a block that is not concrete powder.
[[nodiscard]] std::optional<std::int32_t> concreteFor(const wire::common::Identifier& block);

/// The concrete a landed powder sets into, if the square it stopped in has water in it or beside it.
///
/// parity: `ConcretePowderBlock.onLand`, which is the shape update's question plus the block the
/// falling powder replaced.
[[nodiscard]] std::optional<std::int32_t> landedConcrete(WorldState& world, std::int32_t x, std::int32_t y,
                                                         std::int32_t z, std::int32_t stateId, std::int32_t replaced);

/// What a geyser makes of itself, which has a clock of its own and so is not modelled here.
using GeyserReshape =
    std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

/// Answers the state this block should now be, or nothing when it is none of these.
///
/// `towards` is the side the news arrives from, which a couple of them ask about: a door only comes
/// apart when what changed is the square its other half stands in.
[[nodiscard]] std::optional<std::int32_t> of(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t stateId, const GeyserReshape& geyser = {},
                                             std::optional<registry::BlockFace> towards = std::nullopt);

} // namespace reshapes

} // namespace basalt::server::runtime::world
