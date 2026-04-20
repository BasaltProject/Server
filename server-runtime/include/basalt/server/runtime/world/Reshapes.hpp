#pragma once

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `updateShape`.
namespace reshapes {

/// parity: the list `ChestBlock` covers.
[[nodiscard]] std::span<const std::string_view> chestBlocks();

/// parity: `ConcretePowderBlock.touchesLiquid`.
[[nodiscard]] bool touchesWater(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

[[nodiscard]] std::optional<std::int32_t> concreteFor(const wire::common::Identifier& block);

/// parity: `ConcretePowderBlock.onLand`.
[[nodiscard]] std::optional<std::int32_t> landedConcrete(WorldState& world, std::int32_t x, std::int32_t y,
                                                         std::int32_t z, std::int32_t stateId, std::int32_t replaced);

/// A geyser has a clock of its own and is not modelled here.
using GeyserReshape =
    std::function<std::optional<std::int32_t>(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId)>;

/// `towards` matters to a couple: a door only comes apart when the changed square is its other half's.
[[nodiscard]] std::optional<std::int32_t> of(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                             std::int32_t stateId, const GeyserReshape& geyser = {},
                                             std::optional<registry::BlockFace> towards = std::nullopt);

} // namespace reshapes

} // namespace basalt::server::runtime::world
