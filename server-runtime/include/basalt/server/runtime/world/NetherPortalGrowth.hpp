#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cstdint>
#include <functional>
#include <string_view>

namespace basalt::server::runtime::world {

namespace netherPortalGrowth {

/// parity: `NetherPortalBlock.randomTick`.
[[nodiscard]] entity::ServerEntity*
netherPortal(WorldState& world, const GameRuleValues& gameRules, std::string_view difficulty, bool spawnsPortalPiglins,
             std::int32_t x, std::int32_t y, std::int32_t z, random::RandomSource& random,
             const std::function<std::int32_t()>& allocateEntityId,
             const std::function<bool(std::int32_t, std::int32_t, std::int32_t)>& anyPlayerCloseEnough = {});

} // namespace netherPortalGrowth

} // namespace basalt::server::runtime::world
