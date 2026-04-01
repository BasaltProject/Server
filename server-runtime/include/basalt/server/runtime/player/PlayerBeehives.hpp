#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime {
class MobActionSystem;
}

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

/// parity: `BeehiveBlock.useItemOn`: shears take three honeycombs and a bottle takes honey from a full
/// hive, and unless it is smoked the bees come out after whoever did it. Answers whether it harvested.
bool harvestHive(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                 std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                 const std::optional<storage::StoredItemStack>& held, MobActionSystem* mobs);

} // namespace basalt::server::runtime::player
