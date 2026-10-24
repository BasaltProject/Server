#pragma once

#include <cstdint>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::world {

class WorldState;

enum class Submersion : std::uint8_t { None, Water, Lava };

/// parity: `LivingEntity.baseTick`.
namespace fluids {

inline constexpr std::int32_t kMaxAir = 300;

/// parity: `LivingEntity.shouldTakeDrowningDamage`.
inline constexpr std::int32_t kDrowningAir = -20;

inline constexpr float kDrownDamage = 2.0F;

inline constexpr float kLavaDamage = 4.0F;
inline constexpr std::int32_t kLavaFireTicks = 15 * 20;
inline constexpr float kFireDamage = 1.0F;

/// parity: `Entity.getEyeY`.
[[nodiscard]] double eyeHeight(bool sneaking);

[[nodiscard]] Submersion of(std::int32_t stateId);

/// parity: `Entity.isInLava`.
[[nodiscard]] bool touchesLava(WorldState& world, const entity::ServerEntity& entity);

/// parity: `LivingEntity.decreaseAirSupply`/`increaseAirSupply`.
[[nodiscard]] std::int32_t airAfterTick(std::int32_t air, bool eyesInWater, std::int32_t maxAir = kMaxAir);

} // namespace fluids

} // namespace basalt::server::runtime::world
