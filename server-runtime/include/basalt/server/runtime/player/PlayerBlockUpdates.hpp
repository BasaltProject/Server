#pragma once

#include <cstdint>

namespace basalt::server::runtime::player {

class PlayerSession;

void sendBlockChangeTo(const PlayerSession& player, std::int32_t x, std::int32_t y, std::int32_t z,
                       std::int32_t stateId);

} // namespace basalt::server::runtime::player
