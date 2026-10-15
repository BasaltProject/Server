#pragma once

#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

void playSoundFor(const PlayerSession& player, std::string_view sound, wire::protocol::play::SoundSource source,
                  double x, double y, double z, float volume, float pitch, std::int64_t seed);

} // namespace basalt::server::runtime::player
