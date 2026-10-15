#pragma once

#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

void showParticleFor(const PlayerSession& player, std::string_view particle,
                     const wire::protocol::play::LevelParticlesPacket& packet);

} // namespace basalt::server::runtime::player
