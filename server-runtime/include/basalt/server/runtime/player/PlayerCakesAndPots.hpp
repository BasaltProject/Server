#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class LoadedWorld;
}

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

bool useCakeFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                const std::optional<storage::StoredItemStack>& held, float clickY);

bool useFlowerPotFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held);

} // namespace basalt::server::runtime::player
