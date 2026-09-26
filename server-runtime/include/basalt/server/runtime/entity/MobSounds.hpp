#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/wire/protocol/play/HudPackets.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// How a mob sounds: how often it speaks up on its own, how loud, at what pitch and in which
/// category.
///
/// parity: `Mob.getAmbientSoundInterval`, `LivingEntity.getSoundVolume`, `getVoicePitch` and
/// `Entity.getSoundSource`, with each mob's overrides.
namespace mobSounds {

/// parity: `Mob.getAmbientSoundInterval`: eighty, and longer for animals, water mobs, golems and
/// the horses.
[[nodiscard]] std::int32_t ambientInterval(const ServerEntity& mob);

/// parity: `LivingEntity.getSoundVolume` and its overrides.
[[nodiscard]] float volume(const ServerEntity& mob);

/// parity: `LivingEntity.getVoicePitch`: higher for a baby, and a few mobs with their own.
[[nodiscard]] float voicePitch(const ServerEntity& mob, random::RandomSource& random);

/// parity: `Entity.getSoundSource`: hostile for the monsters, neutral for the rest.
[[nodiscard]] wire::protocol::play::SoundSource source(const ServerEntity& mob);

/// The sound of this `kind` (`ambient`, `hurt`, `death`, `pant` and so on) from the sound variant
/// of a cow, pig, chicken, cat or wolf, a baby's own where it has one. Empty for any other mob.
///
/// parity: the `*_sound_variant` registries' `adult_sounds` and `baby_sounds`.
[[nodiscard]] std::string variantSound(const ServerEntity& mob, std::string_view kind);

} // namespace mobSounds

} // namespace basalt::server::runtime::entity
