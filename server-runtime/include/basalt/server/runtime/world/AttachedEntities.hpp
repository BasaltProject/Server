#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/Cushions.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/server/world/loot/LootContext.hpp>

#include <functional>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::world {

class LoadedWorld;

namespace attachedEntities {

inline constexpr std::int32_t kCheckInterval = 100;

[[nodiscard]] bool isAttached(const wire::common::Identifier& type);

[[nodiscard]] bool dueForCheck(entity::ServerEntity& attached);

[[nodiscard]] bool survives(const WorldState& world, const entity::ServerEntity& attached,
                            const cushions::Tagged& tagged);

[[nodiscard]] std::vector<server::world::loot::ItemStack> drops(const entity::ServerEntity& attached,
                                                                random::RandomSource& random);

[[nodiscard]] std::string_view breakSound(const wire::common::Identifier& type);

void comeOff(LoadedWorld& world, entity::ServerEntity& attached, const std::vector<player::PlayerSession*>& watchers,
             random::RandomSource& random, const std::function<std::int32_t()>& allocateEntityId);

} // namespace attachedEntities

} // namespace basalt::server::runtime::world
