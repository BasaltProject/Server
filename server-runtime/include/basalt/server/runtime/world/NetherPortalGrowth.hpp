#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `NetherPortalBlock.randomTick`, zombified piglins spawning in an overworld portal.
namespace netherPortalGrowth {

/// parity: `NetherPortalBlock.randomTick`, where an overworld portal's zombified piglins come from.
/// `spawnsPortalPiglins` comes from the dimension type, not from here.
///
/// `anyPlayerCloseEnough` answers vanilla's `anyPlayerCloseEnoughForSpawning`, which needs players
/// that do not exist yet: unset means yes, so the roll before it stays the last word.
[[nodiscard]] entity::ServerEntity*
netherPortal(WorldState& world, const GameRuleValues& gameRules, std::string_view difficulty, bool spawnsPortalPiglins,
             std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
             const std::function<std::int32_t()>& allocateEntityId,
             const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& anyPlayerCloseEnough = {});

} // namespace netherPortalGrowth

} // namespace basalt::server::runtime::world
