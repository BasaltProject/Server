#pragma once

#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>

namespace basalt::server::runtime::world {

/// The block that moves one item at a time, which is where redstone and containers meet.
///
/// parity: `HopperBlockEntity.tryMoveItems`: nothing happens while it is on cooldown or while its
/// `enabled` is false, it pushes one item into whatever it faces, pulls one out of whatever is above
/// it, and any move at all puts it back on an eight tick cooldown.
///
/// A hopper also eats what is lying above it, which is `suckInItems`' own box.
namespace hoppers {

/// parity: `HopperBlockEntity.HOPPER_CONTAINER_SIZE`.
inline constexpr std::int32_t kSize = 5;

/// One hopper's turn. Answers whether anything moved, so the caller marks the chunk and tells
/// whoever is looking at the two containers.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z);

} // namespace hoppers

} // namespace basalt::server::runtime::world
