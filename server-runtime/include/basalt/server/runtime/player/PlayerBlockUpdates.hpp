#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Tells one player that one block changed, whichever edition they are on. A connection that knows
/// how to say it says it; one that only takes a Java packet gets that packet, built here. The second
/// half stops a connection written before this existed from losing block updates in silence.
void sendBlockChangeTo(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId);

} // namespace basalt::server::runtime::player
