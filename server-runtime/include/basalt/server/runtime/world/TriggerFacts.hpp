#pragma once

#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {

class LoadedWorld;
class RealLootTables;

/// What the advancement and loot predicates can see of the things in the world.
namespace triggerFacts {

/// parity: the parts of `Entity` an `EntityPredicate` reads, for a mob or any other entity.
[[nodiscard]] server::world::loot::EntityFacts ofEntity(const LoadedWorld& world, const entity::ServerEntity& entity);

/// The same without the world around it, so without what it rides.
[[nodiscard]] server::world::loot::EntityFacts ofEntity(const entity::ServerEntity& entity);

/// The same for a player: what they wear and hold, their flags and effects.
[[nodiscard]] server::world::loot::EntityFacts ofPlayer(const player::PlayerSession& player);

/// The damage type behind the name `entityCombat::hurt` was given, where the two differ.
[[nodiscard]] wire::common::Identifier damageTypeOf(std::string_view cause);

[[nodiscard]] std::optional<TriggerItem> itemOf(const std::optional<storage::StoredItemStack>& stack);

/// A player's predicates' context: their facts as `this`, prepared at where they stand.
[[nodiscard]] server::world::loot::LootContext ofPlayerAt(RealLootTables& tables, LoadedWorld& world,
                                                          const player::PlayerSession& player);

/// parity: `ItemUsedOnLocationTrigger`'s context: the player, the block, the tool and the square's
/// centre. `stateId` stands in for the block when it has changed since the use it answers.
[[nodiscard]] server::world::loot::LootContext atBlock(RealLootTables& tables, LoadedWorld& world,
                                                       const player::PlayerSession& player, std::int32_t x,
                                                       std::int32_t y, std::int32_t z,
                                                       const std::optional<storage::StoredItemStack>& tool,
                                                       std::optional<std::int32_t> stateId = std::nullopt);

/// A player's facts, or an entity's, by entity id in `world`.
[[nodiscard]] std::optional<server::world::loot::EntityFacts>
ofId(const LoadedWorld& world, const std::vector<player::PlayerSession*>& players, std::int32_t id);

/// parity: `PlayerHurtEntityTrigger`, fired for a player's hit on `victim`. `direct` is what landed
/// it when that was not the player's own hand.
void playerHurtEntity(AdvancementTriggers& triggers, const LoadedWorld& world, const player::PlayerSession& player,
                      const entity::ServerEntity& victim, float dealt, const wire::common::Identifier& damageType,
                      const std::optional<server::world::loot::EntityFacts>& direct = std::nullopt);

} // namespace triggerFacts
} // namespace basalt::server::runtime::world
