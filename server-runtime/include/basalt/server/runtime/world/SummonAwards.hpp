#pragma once

#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

class AdvancementTriggers;

/// parity: `CarvedPumpkinBlock.spawnGolemInWorld`.
namespace summonAwards {

inline constexpr double kGolemRange = 5.0;
inline constexpr double kWitherRange = 50.0;

void award(AdvancementTriggers* triggers, const std::vector<player::PlayerSession*>& players,
           std::string_view worldName, const entity::ServerEntity& summoned, double range);

} // namespace summonAwards

} // namespace basalt::server::runtime::world
