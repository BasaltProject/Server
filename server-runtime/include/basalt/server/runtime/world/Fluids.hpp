#pragma once

#include <cstdint>

namespace basalt::server::runtime::world {

/// What a player is standing in, which is all the drowning and burning rules need to know.
enum class Submersion : std::uint8_t { None, Water, Lava };

/// Drowning and burning, the two things being under something does to a player.
///
/// parity: `LivingEntity.baseTick` for the air supply, `Entity.lavaHurt` for the lava. Both are the
/// server's job: the client draws the bubbles from the air supply it is sent and the flames from the
/// shared flags, and does neither on its own.
namespace fluids {

/// parity: `Entity.getMaxAirSupply`, the three hundred ticks of air the bar is drawn from.
inline constexpr std::int32_t kMaxAir = 300;

/// parity: `LivingEntity.shouldTakeDrowningDamage`: air runs twenty ticks past empty first.
inline constexpr std::int32_t kDrowningAir = -20;

inline constexpr float kDrownDamage = 2.0F;

/// parity: `Entity.lavaHurt`, four hearts' worth and fifteen seconds alight.
inline constexpr float kLavaDamage = 4.0F;
inline constexpr std::int32_t kLavaFireTicks = 15 * 20;
inline constexpr float kFireDamage = 1.0F;

/// parity: `Entity.getEyeY`, near enough: this build has no poses, so a sneaking player is the low
/// one.
[[nodiscard]] double eyeHeight(bool sneaking);

[[nodiscard]] Submersion of(std::int32_t stateId);

/// parity: `LivingEntity.decreaseAirSupply`/`increaseAirSupply`: one tick down, four back up.
[[nodiscard]] std::int32_t airAfterTick(std::int32_t air, bool eyesInWater);

} // namespace fluids

} // namespace basalt::server::runtime::world
