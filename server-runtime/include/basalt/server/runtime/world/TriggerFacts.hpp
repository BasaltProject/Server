#pragma once

#include <basalt/server/runtime/world/AdvancementTriggers.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/server/world/loot/LootContext.hpp>
#include <basalt/server/world/loot/LootPredicates.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {

class LoadedWorld;
class RealLootTables;

namespace triggerFacts {

/// parity: the parts of `Entity` an `EntityPredicate` reads.
[[nodiscard]] server::world::loot::EntityFacts ofEntity(const LoadedWorld& world, const entity::ServerEntity& entity);

[[nodiscard]] server::world::loot::EntityFacts ofEntity(const entity::ServerEntity& entity);

[[nodiscard]] server::world::loot::EntityFacts ofPlayer(const player::PlayerSession& player);

[[nodiscard]] wire::common::Identifier damageTypeOf(std::string_view cause);

[[nodiscard]] std::optional<TriggerItem> itemOf(const std::optional<storage::StoredItemStack>& stack);

[[nodiscard]] server::world::loot::LootContext ofPlayerAt(RealLootTables& tables, LoadedWorld& world,
                                                          const player::PlayerSession& player,
                                                          const player::PlayerRegistry* players = nullptr);

/// parity: `PlayerPredicate.looking_at`.
[[nodiscard]] std::shared_ptr<const server::world::loot::EntityFacts>
lookingAtOf(LoadedWorld& world, const player::PlayerSession& player, const player::PlayerRegistry* players);

/// parity: `ItemUsedOnLocationTrigger`.
[[nodiscard]] server::world::loot::LootContext atBlock(RealLootTables& tables, LoadedWorld& world,
                                                       const player::PlayerSession& player, std::int32_t x,
                                                       std::int32_t y, std::int32_t z,
                                                       const std::optional<storage::StoredItemStack>& tool,
                                                       std::optional<std::int32_t> stateId = std::nullopt);

[[nodiscard]] vibrations::Source vibrationSourceOf(const player::PlayerSession& player, AdvancementTriggers* triggers);

[[nodiscard]] std::optional<server::world::loot::EntityFacts>
ofId(const LoadedWorld& world, const std::vector<player::PlayerSession*>& players, std::int32_t id);

/// parity: `PlayerHurtEntityTrigger`.
void playerHurtEntity(AdvancementTriggers& triggers, const LoadedWorld& world, const player::PlayerSession& player,
                      const entity::ServerEntity& victim, float dealt, const wire::common::Identifier& damageType,
                      const std::optional<server::world::loot::EntityFacts>& direct = std::nullopt);

/// parity: `ItemStack.applyDamage`.
void durabilityChanged(AdvancementTriggers* triggers, const player::PlayerSession& player,
                       const storage::StoredItemStack& before, const std::optional<storage::StoredItemStack>& after);

} // namespace triggerFacts
} // namespace basalt::server::runtime::world
