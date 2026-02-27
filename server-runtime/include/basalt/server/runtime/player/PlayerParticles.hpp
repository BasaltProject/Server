#pragma once

#include <basalt/wire/protocol/play/EntityPackets.hpp>

#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Shows a particle to one player, whichever edition they are on.
///
/// The Java packet comes already built because only the caller knows its options; the name is what
/// another edition needs. Like `playSoundFor`, a connection that cannot take Java's packet still
/// gets the particle if it knows how to draw one.
void showParticleFor(const PlayerSession& player, std::string_view particle,
                     const wire::protocol::play::LevelParticlesPacket& packet);

} // namespace basalt::server::runtime::player
