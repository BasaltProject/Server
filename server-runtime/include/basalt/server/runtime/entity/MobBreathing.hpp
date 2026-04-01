#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace basalt::server::random {
class RandomSource;
}

namespace basalt::server::runtime::entity {

class ServerEntity;

/// How a mob runs out of air: underwater for most, out of it for what lives there.
///
/// parity: the air half of `LivingEntity.baseTick`, and the `handleAirSupply` of `WaterAnimal`,
/// `AgeableWaterCreature`, `Axolotl` and `Nautilus`.
namespace mobBreathing {

/// What the mob's air depends on this tick.
struct Surroundings {
    /// parity: `isEyeInFluid(WATER)`, and whether that square is a bubble column.
    bool eyeInWater = false;
    bool eyeInBubbles = false;
    /// parity: `Entity.isInWater` and `isInRain`.
    bool inWater = false;
    bool inRain = false;
    /// parity: `Attributes.OXYGEN_BONUS`, and the roll that spares a breath with it. Null never rolls.
    double oxygenBonus = 0.0;
    random::RandomSource* random = nullptr;
};

/// One hit the air owes it this tick.
struct Hurt {
    float amount = 0.0F;
    /// `drown` or `dry_out`.
    std::string_view cause;
    /// parity: `EntityEvent.DROWN_PARTICLES`, which goes out just before a drowning hit.
    bool drownParticles = false;
};

/// parity: `getMaxAirSupply`, 300 but for the dolphin and the axolotl.
[[nodiscard]] std::int32_t maxAirOf(std::string_view typePath);

/// parity: `#minecraft:can_breathe_under_water`.
[[nodiscard]] bool breathesUnderwater(std::string_view typePath);

/// The mob's eye height above its feet. parity: `LivingEntity.getDimensions(pose).eyeHeight`: the
/// builder's eye, a baby's own, a cube's size, the sleeping pose and the `scale` attribute.
[[nodiscard]] double eyeHeightOf(const ServerEntity& mob);

/// Moves the air on by a tick and answers what that costs.
[[nodiscard]] std::vector<Hurt> tick(ServerEntity& mob, const Surroundings& at);

} // namespace mobBreathing

} // namespace basalt::server::runtime::entity
