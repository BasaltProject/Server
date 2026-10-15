#pragma once

#include <basalt/server/random/RandomSource.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity {

class ServerEntity;

namespace parrots {

/// parity: `ShoulderRidingEntity.RIDE_COOLDOWN`.
inline constexpr std::int32_t kRideCooldown = 100;

[[nodiscard]] std::optional<std::string_view> imitationOf(std::string_view mobType);

/// parity: `Parrot.getAmbient`.
[[nodiscard]] std::string ambientSound(bool peaceful, random::RandomSource& random);

[[nodiscard]] float pitch(random::RandomSource& random);

[[nodiscard]] bool canSitOnShoulder(const ServerEntity& parrot);

} // namespace parrots

} // namespace basalt::server::runtime::entity
