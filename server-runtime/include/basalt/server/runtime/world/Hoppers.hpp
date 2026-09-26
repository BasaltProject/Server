#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/world/BlockChange.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/crafting/Crafting.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

/// The hopper. parity: `HopperBlockEntity.tryMoveItems`: idle while on cooldown or not `enabled`,
/// it pushes one item into what it faces, pulls one from above (plus loose items in `suckInItems`'
/// box), and any move puts it on an eight tick cooldown.
namespace hoppers {

/// parity: `HopperBlockEntity.HOPPER_CONTAINER_SIZE`.
inline constexpr std::int32_t kSize = 5;

/// What a hopper needs from outside the block world, and what it hands back.
struct Around {
    /// parity: `ChiseledBookShelfBlockEntity.acceptsItemType`, the one container door that asks an
    /// item **tag**. The tags come from the datapack, so the caller answers.
    std::function<bool(const wire::common::Identifier& item)> isBookshelfBook;
    /// parity: the `does_not_block_hoppers` block tag, which lets a full block above still be sucked
    /// through. Unset, no block is in it.
    std::function<bool(const wire::common::Identifier& block)> doesNotBlockHoppers;
    /// parity: `AbstractFurnaceBlockEntity.setItem`, which is what a hopper filling an **empty**
    /// slot of a furnace goes through: the cooking time is set from what went in. The recipe book
    /// belongs to the world, so the caller answers it.
    std::function<const server::world::crafting::Recipe*(server::world::crafting::Cooker,
                                                         const server::world::loot::ItemStack&)>
        recipeFor;
    /// The blocks whose state a move changed, for whoever tells the clients: a jukebox that has just
    /// been loaded and a bookshelf whose slot has just filled both look different afterwards.
    std::vector<BlockChange>* changes = nullptr;
    /// parity: `ShelfBlockEntity.setChanged`, the one container that makes a noise when a hopper
    /// touches it. A vibration is booked for the tick it arrives, so the turn has to be known.
    std::int64_t gameTime = 0;
};

/// One hopper's turn, answering whether anything moved. `random` is the world's and is required
/// rather than reached for: a hopper feeding a composter rolls the item's own chance, which is the
/// one place a hopper is not deterministic.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                        const Around& around = {});

/// One hopper minecart's turn. parity: `MinecartHopper.tryConsumeItems` over
/// `HopperBlockEntity.suckInItems`: one item a tick with no cooldown, never pushes, asks about the
/// square above its own middle (half over a chest still counts), and also picks up items in its own box.
[[nodiscard]] bool tickCart(WorldState& world, entity::ServerEntity& cart, std::int64_t gameTime);

} // namespace hoppers

} // namespace basalt::server::runtime::world
