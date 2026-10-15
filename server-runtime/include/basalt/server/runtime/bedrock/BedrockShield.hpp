#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::bedrock::shield {

void tick(player::PlayerSession& player, std::int64_t gameTime, const std::vector<player::PlayerSession*>& watchers);

} // namespace basalt::server::runtime::bedrock::shield
