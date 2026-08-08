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

/// parity: `HopperBlockEntity.tryMoveItems`.
namespace hoppers {

inline constexpr std::int32_t kSize = 5;

/// parity: `HopperBlockEntity.MOVE_ITEM_SPEED`.
inline constexpr std::int32_t kVanillaCooldown = 8;

struct Around {
    /// parity: `ChiseledBookShelfBlockEntity.acceptsItemType`.
    std::function<bool(const wire::common::Identifier& item)> isBookshelfBook;
    /// parity: `does_not_block_hoppers`.
    std::function<bool(const wire::common::Identifier& block)> doesNotBlockHoppers;
    /// parity: `AbstractFurnaceBlockEntity.setItem`.
    std::function<const server::world::crafting::Recipe*(server::world::crafting::Cooker,
                                                         const server::world::loot::ItemStack&)>
        recipeFor;
    std::vector<BlockChange>* changes = nullptr;
    /// parity: `ShelfBlockEntity.setChanged`.
    std::int64_t gameTime = 0;
    std::int32_t transferTicks = kVanillaCooldown;
    std::int32_t amount = 1;
};

/// `random` is required: a hopper feeding a composter rolls the item's chance, the one non-deterministic spot.
[[nodiscard]] bool tick(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
                        const Around& around = {});

/// parity: `MinecartHopper.tryConsumeItems`.
[[nodiscard]] bool tickCart(WorldState& world, entity::ServerEntity& cart, std::int64_t gameTime);

} // namespace hoppers

} // namespace basalt::server::runtime::world
