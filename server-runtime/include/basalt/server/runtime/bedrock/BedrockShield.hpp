#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {
class PlayerSession;
}

namespace basalt::server::runtime::bedrock::shield {

/// One turn of a Bedrock player's shield, on the world's thread. Bedrock raises a held shield by
/// sneaking where Java asks with a use, so the sneak is what starts and ends the block here; the
/// count it keeps is the same one a Java player's raised shield keeps.
void tick(player::PlayerSession& player, std::int64_t gameTime);

} // namespace basalt::server::runtime::bedrock::shield
