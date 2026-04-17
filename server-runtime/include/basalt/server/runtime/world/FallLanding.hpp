#pragma once

#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <string_view>

namespace basalt::server::runtime::world {

/// What the block a fall ends on does to it.
///
/// parity: `Block.fallOn` and its overrides: 26.3's `fallDistanceReduction` property (beds and the
/// shelf mushroom), the hay and honey blocks' fifth, slime's bounce, and a stalagmite's spike.
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

/// The landing on `stateId`. `suppressingBounce` is a sneaking player, whom slime does not spare.
[[nodiscard]] Landing of(std::int32_t stateId, bool suppressingBounce);

/// parity: `LivingEntity.calculateFallDamage`: the power past the safe distance, times the multiplier,
/// floored.
[[nodiscard]] std::int32_t damage(double fallDistance, const Landing& landing, double safeFallDistance,
                                  double fallDamageMultiplier = 1.0);

/// parity: `ShelfMushroomBlock.bounceOn`, whose bounce sound leaves out exactly the two entities it
/// checks for, the item entity and primed tnt.
[[nodiscard]] bool isSilentBouncer(const wire::common::Identifier& entityType);

} // namespace fallLanding

} // namespace basalt::server::runtime::world
