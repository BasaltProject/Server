#pragma once

#include <basalt/server/registry/BlockShapes.hpp>

#include <cstdint>
#include <optional>
#include <string_view>

namespace basalt::server::registry {

/// Which blocks reach out to what is beside them: fences, gates, panes and bars.
///
/// parity: `FenceBlock.connectsTo` and `IronBarsBlock.attachsTo`. What each block *is* comes from
/// the jar (a `FenceBlock`, the `#minecraft:fences` tag and the rest are in the generated table), so
/// this is the rule and not the data.
///
/// Walls also connect and are not here: their sides are none, low or tall depending on what stands
/// above them, which is a different question from this one.
namespace blockConnections {

/// Whether this block connects at all, and so has north/south/east/west to keep up to date.
[[nodiscard]] bool connects(std::int32_t stateId);

[[nodiscard]] bool isFence(std::int32_t stateId);
[[nodiscard]] bool isPanes(std::int32_t stateId);
[[nodiscard]] bool isWall(std::int32_t stateId);
[[nodiscard]] bool isLeaves(std::int32_t stateId);
[[nodiscard]] bool isStairs(std::int32_t stateId);

/// parity: `#minecraft:prevents_nearby_leaf_decay`, which is what keeps a canopy on a trunk.
[[nodiscard]] bool preventsLeafDecay(std::int32_t stateId);

/// parity: `#minecraft:maintains_farmland`, the crops that keep tilled soil from reverting.
[[nodiscard]] bool maintainsFarmland(std::int32_t stateId);

/// parity: `#minecraft:causes_continuous_geyser_eruptions`, which is lava.
[[nodiscard]] bool eruptsContinuously(std::int32_t stateId);

/// parity: `#minecraft:causes_periodic_geyser_eruptions`, which is a magma block.
[[nodiscard]] bool eruptsPeriodically(std::int32_t stateId);

/// parity: `BlockState.hasBlockEntity`, which is the last thing a piston asks before pushing.
[[nodiscard]] bool hasBlockEntity(std::int32_t stateId);

/// parity: `#minecraft:sniffer_egg_hatch_boost`, the moss that halves an egg's wait.
[[nodiscard]] bool boostsHatching(std::int32_t stateId);

/// parity: `#minecraft:supports_cactus`, which is sand and little else.
[[nodiscard]] bool supportsCactus(std::int32_t stateId);

/// parity: `FallingBlock`, the sand, gravel, powders, anvils and dripstone that come down.
[[nodiscard]] bool isFalling(std::int32_t stateId);

/// parity: `#minecraft:soul_fire_base_blocks`, the ground that burns blue.
[[nodiscard]] bool isSoulFireBase(std::int32_t stateId);

/// The corner a flight of stairs turns, from the stairs in front of and behind it.
///
/// parity: `StairBlock.getStairsShape`. Answers the `shape` value, or absent when the block is not
/// stairs at all. Neighbours are given as the states around it.
[[nodiscard]] std::optional<std::string_view> stairShape(std::int32_t stateId, std::int32_t north, std::int32_t south,
                                                         std::int32_t west, std::int32_t east);

/// Whether `stateId` reaches towards its neighbour, which lies in `face`.
///
/// parity: the two rules are nearly the same, and both start with a sturdy face on the neighbour,
/// which is why a fence grabs a wall of stone and not a wall of leaves.
[[nodiscard]] bool connectsTo(std::int32_t stateId, std::int32_t neighbourStateId, BlockFace face);

} // namespace blockConnections

} // namespace basalt::server::registry
