#pragma once

#include <cstdint>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::bedrock::shield {

/// One turn of a Bedrock player's shield, on the world's thread. Bedrock raises a shield by sneaking
/// rather than with a use, so the sneak starts and ends the block; `watchers` are told when it goes up
/// or down, through the living flags a Java client draws a raised shield from.
void tick(player::PlayerSession& player, std::int64_t gameTime, const std::vector<player::PlayerSession*>& watchers);

} // namespace basalt::server::runtime::bedrock::shield
