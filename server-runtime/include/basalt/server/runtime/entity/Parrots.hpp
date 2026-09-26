#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

/// The parrot: the mobs it imitates and the shoulder it sits on.
namespace parrots {

/// parity: `ShoulderRidingEntity.RIDE_COOLDOWN`, ticks in the world before it will sit on a shoulder.
inline constexpr std::int32_t kRideCooldown = 100;

/// parity: `Parrot.MOB_SOUND_MAP`: the sound a parrot makes for that mob, empty for the happy ghast's
/// silent one, nothing for a mob it does not imitate.
[[nodiscard]] std::optional<std::string_view> imitationOf(std::string_view mobType);

/// parity: `Parrot.getAmbient`: one call in a thousand borrowed from a monster, never in peace. The
/// jar picks it off a hash map, so which monster comes with which roll is not the jar's.
[[nodiscard]] std::string ambientSound(bool peaceful, random::RandomSource& random);

/// parity: `Parrot.getPitch`.
[[nodiscard]] float pitch(random::RandomSource& random);

/// parity: `ShoulderRidingEntity.canSitOnShoulder`, `rideCooldownCounter` being its ticks lived.
[[nodiscard]] bool canSitOnShoulder(const ServerEntity& parrot);

} // namespace parrots

} // namespace basalt::server::runtime::entity
