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

/// Who gets credit for something a pattern of blocks just raised.
///
/// parity: `CarvedPumpkinBlock.spawnGolemInWorld` and `WitherSkullBlock.checkSpawn` do not look for
/// whoever placed the last block: they hand the advancement to every player standing near what came
/// out, so a bystander gets it and a builder who walked away does not.
namespace summonAwards {

/// parity: `golem.getBoundingBox().inflate(5.0)`.
inline constexpr double kGolemRange = 5.0;
/// parity: `witherBoss.getBoundingBox().inflate(50.0)`.
inline constexpr double kWitherRange = 50.0;

void award(AdvancementTriggers* triggers, const std::vector<player::PlayerSession*>& players,
           std::string_view worldName, const entity::ServerEntity& summoned, double range);

} // namespace summonAwards

} // namespace basalt::server::runtime::world
