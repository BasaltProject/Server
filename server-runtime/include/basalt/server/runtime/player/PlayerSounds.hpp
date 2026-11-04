#pragma once

#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <span>
#include <string_view>

namespace basalt::server::runtime::player {

class PlayerSession;

void playSoundFor(const PlayerSession& player, std::string_view sound, wire::protocol::play::SoundSource source,
                  double x, double y, double z, float volume, float pitch, std::int64_t seed);

/// parity: `Player.giveExperienceLevels` plays `PLAYER_LEVELUP` at multiples of five with a 100-tick cooldown.
void playLevelUpSoundIfDue(PlayerSession& player, std::span<PlayerSession* const> watchers, std::int64_t seed = 0);

} // namespace basalt::server::runtime::player
