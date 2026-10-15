#pragma once

#include <basalt/server/registry/BlockShapes.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::registry {
// parity: `FenceBlock.connectsTo` and `IronBarsBlock.attachsTo`.
namespace blockConnections {
[[nodiscard]] bool connects(std::int32_t stateId);

[[nodiscard]] bool isFence(std::int32_t stateId);
[[nodiscard]] bool isPanes(std::int32_t stateId);
[[nodiscard]] bool isWall(std::int32_t stateId);
[[nodiscard]] bool isLeaves(std::int32_t stateId);
[[nodiscard]] bool isStairs(std::int32_t stateId);

// parity: `#minecraft:prevents_nearby_leaf_decay`.
[[nodiscard]] bool preventsLeafDecay(std::int32_t stateId);

// parity: `#minecraft:maintains_farmland`, the crops that keep tilled soil from reverting.
[[nodiscard]] bool maintainsFarmland(std::int32_t stateId);

// parity: `#minecraft:causes_continuous_geyser_eruptions`, which is lava.
[[nodiscard]] bool eruptsContinuously(std::int32_t stateId);

// parity: `#minecraft:causes_periodic_geyser_eruptions`, which is a magma block.
[[nodiscard]] bool eruptsPeriodically(std::int32_t stateId);

// parity: `BlockState.hasBlockEntity`.
[[nodiscard]] bool hasBlockEntity(std::int32_t stateId);

// parity: `#minecraft:sniffer_egg_hatch_boost`, the moss that halves an egg's wait.
[[nodiscard]] bool boostsHatching(std::int32_t stateId);

// parity: `#minecraft:supports_cactus`, which is sand and little else.
[[nodiscard]] bool supportsCactus(std::int32_t stateId);

// parity: `FallingBlock`, the sand, gravel, powders, anvils and dripstone that come down.
[[nodiscard]] bool isFalling(std::int32_t stateId);

// parity: `#minecraft:soul_fire_base_blocks`, the ground that burns blue.
[[nodiscard]] bool isSoulFireBase(std::int32_t stateId);

// parity: `StairBlock.getStairsShape`.
[[nodiscard]] std::optional<std::string_view> stairShape(std::int32_t stateId, std::int32_t north, std::int32_t south,
                                                         std::int32_t west, std::int32_t east);

// parity: `FenceBlock.connectsTo` and `IronBarsBlock.attachsTo` share this rule.
[[nodiscard]] bool connectsTo(std::int32_t stateId, std::int32_t neighbourStateId, BlockFace face);
} // namespace blockConnections
} // namespace basalt::server::registry
