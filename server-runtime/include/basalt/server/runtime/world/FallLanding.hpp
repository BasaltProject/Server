#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world {

/// parity: `Block.fallOn`.
namespace fallLanding {

/// parity: `Entity.getOnPosLegacy`, the block a fifth of a block under the feet.
inline constexpr double kProbeDepth = 0.2000000029802322;

struct Landing {
    /// parity: `fallDistance * (1 - getFallDistanceReduction())`.
    double distanceScale = 1.0;
    /// parity: `PointedDripstoneBlock.fallOn`'s two and a half blocks more.
    double extraDistance = 0.0;
    /// parity: the `damageModifier` handed to `causeFallDamage`.
    float damageMultiplier = 1.0F;
    /// parity: `DamageSources.stalagmite` rather than `fall`.
    bool stalagmite = false;
};

/// `suppressingBounce` is a sneaking player, whom slime does not spare.
[[nodiscard]] Landing of(std::int32_t stateId, bool suppressingBounce);

/// parity: `LivingEntity.calculateFallDamage`.
[[nodiscard]] std::int32_t damage(double fallDistance, const Landing& landing, double safeFallDistance,
                                  double fallDamageMultiplier = 1.0);

/// parity: `ShelfMushroomBlock.bounceOn`.
[[nodiscard]] bool isSilentBouncer(const wire::common::Identifier& entityType);

} // namespace fallLanding

} // namespace basalt::server::runtime::world
