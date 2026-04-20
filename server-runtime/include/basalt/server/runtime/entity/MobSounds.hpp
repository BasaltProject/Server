#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace mobSounds {

[[nodiscard]] std::int32_t ambientInterval(const ServerEntity& mob);

[[nodiscard]] float volume(const ServerEntity& mob);

[[nodiscard]] float voicePitch(const ServerEntity& mob, random::RandomSource& random);

[[nodiscard]] wire::protocol::play::SoundSource source(const ServerEntity& mob);

[[nodiscard]] std::string variantSound(const ServerEntity& mob, std::string_view kind);

} // namespace mobSounds

} // namespace basalt::server::runtime::entity
