#pragma once

#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Identifier.hpp>
#include <basalt/wire/protocol/play/BlockPos.hpp>

#include <cstdint>
#include <optional>

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealBlockTransformers;
class RealRecipes;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::player {

struct BreakContext;
class PlayerSession;

bool useBoneMealFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                    std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

bool useHoneycombFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

void primeTntFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                 std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held,
                 bool wears);

void igniteFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
               std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

bool useAxeFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
               std::int32_t y, std::int32_t z, std::int32_t heldSlot, const storage::StoredItemStack& held);

bool transformBlockFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world,
                       world::RealBlockTransformers& transformers, const wire::common::Identifier& transformer,
                       std::int32_t x, std::int32_t y, std::int32_t z, wire::protocol::play::Direction face,
                       std::int32_t heldSlot, const storage::StoredItemStack& held);

bool useComposterFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                     std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                     const std::optional<storage::StoredItemStack>& held);

bool placeOnCampfireFor(const BreakContext& context, PlayerSession& player, world::LoadedWorld& world, std::int32_t x,
                        std::int32_t y, std::int32_t z, std::int32_t heldSlot,
                        const std::optional<storage::StoredItemStack>& held, world::RealRecipes* recipes);

} // namespace basalt::server::runtime::player
