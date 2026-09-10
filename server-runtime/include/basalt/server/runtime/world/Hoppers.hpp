#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <vector>

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

/// What a hopper needs from outside the block world, and what it hands back.
struct Around {
    /// parity: `ChiseledBookShelfBlockEntity.acceptsItemType`, the one container door that asks an
    /// item **tag**. The tags come from the datapack, so the caller answers.
    std::function<bool(const wire::common::Identifier& item)> isBookshelfBook;
    /// The blocks whose state a move changed, for whoever tells the clients: a jukebox that has just
    /// been loaded and a bookshelf whose slot has just filled both look different afterwards.
    std::vector<BlockChange>* changes = nullptr;
};

/// One hopper's turn. Answers whether anything moved, so the caller marks the chunk and tells
/// whoever is looking at the two containers.
///
/// `random` is the world's, and it is a required parameter rather than something this reaches for:
/// a hopper feeding a **composter** rolls the item's own chance, which is the one place a hopper is
/// not deterministic.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                        random::RandomSource& random, const Around& around = {});

/// One hopper minecart's turn, which is the same suck and nothing else.
///
/// parity: `MinecartHopper.tryConsumeItems` over `HopperBlockEntity.suckInItems`. A cart takes one
/// item a tick with **no cooldown**, never gives anything away, and asks about the square above its
/// own middle rather than about a grid square, so half over a chest still counts. It also picks up
/// what is lying in its own box, which a block hopper does not.
[[nodiscard]] bool tickCart(WorldState& world, entity::ServerEntity& cart);

} // namespace hoppers

} // namespace basalt::server::runtime::world
