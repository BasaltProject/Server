#pragma once

#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

/// Plays a sound for one player, whichever edition they are on.
///
/// Like `sendBlockChangeTo`, a connection that cannot take a packet of Java's still gets the sound
/// if it knows how to play one: splitting that in two is what loses things quietly.
void playSoundFor(const PlayerSession& player, std::string_view sound, wire::protocol::play::SoundSource source,
                  double x, double y, double z, float volume, float pitch, std::int64_t seed);

} // namespace basalt::server::runtime::player
