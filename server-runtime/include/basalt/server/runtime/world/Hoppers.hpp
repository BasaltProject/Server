#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

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
///
/// `random` is the world's, and it is a required parameter rather than something this reaches for:
/// a hopper feeding a **composter** rolls the item's own chance, which is the one place a hopper is
/// not deterministic.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                        random::RandomSource& random);

/// One hopper minecart's turn, which is the same suck and nothing else.
///
/// parity: `MinecartHopper.tryConsumeItems` over `HopperBlockEntity.suckInItems`. A cart takes one
/// item a tick with **no cooldown**, never gives anything away, and asks about the square above its
/// own middle rather than about a grid square, so half over a chest still counts. It also picks up
/// what is lying in its own box, which a block hopper does not.
[[nodiscard]] bool tickCart(WorldState& world, entity::ServerEntity& cart);

} // namespace hoppers

} // namespace basalt::server::runtime::world
