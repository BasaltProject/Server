#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/FallLanding.hpp>

#include <cmath>
#include <map>
#include <string>

namespace basalt::server::runtime::world::fallLanding {
namespace {

/// parity: `BlockBehaviour.Properties.fallDistanceReduction`.
constexpr double kHalfReduction = 0.5;

/// parity: `HayBlock.fallOn` and `HoneyBlock.fallOn`, a fifth of the damage.
constexpr float kSoftLanding = 0.2F;

/// parity: `PointedDripstoneBlock.fallOn`.
constexpr double kSpikeExtra = 2.5;
constexpr float kSpikeMultiplier = 2.0F;

/// parity: `calculateFallPower`'s epsilon.
constexpr double kEpsilon = 1.0E-6;

} // namespace

bool isSilentBouncer(const wire::common::Identifier& entityType) {
    return entityType.path() == "item" || entityType.path() == "tnt";
}

Landing of(std::int32_t stateId, bool suppressingBounce) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    if (block == nullptr) {
        return {};
    }
    const std::string_view path = block->path();
    if ((path.ends_with("_bed") && path != "straw_bed") || path == "shelf_mushroom") {
        return Landing{.distanceScale = 1.0 - kHalfReduction};
    }
    if (path == "hay_block" || path == "honey_block") {
        return Landing{.damageMultiplier = kSoftLanding};
    }
    // parity: `SlimeBlock.fallOn`.
    if (path == "slime_block") {
        return suppressingBounce ? Landing{} : Landing{.damageMultiplier = 0.0F};
    }
    if (path == "pointed_dripstone") {
        const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
        const auto direction = properties.find("vertical_direction");
        const auto thickness = properties.find("thickness");
        if (direction != properties.end() && direction->second == "up" && thickness != properties.end() &&
            thickness->second == "tip") {
            return Landing{.extraDistance = kSpikeExtra, .damageMultiplier = kSpikeMultiplier, .stalagmite = true};
        }
    }
    return {};
}

std::int32_t damage(double fallDistance, const Landing& landing, double safeFallDistance, double fallDamageMultiplier) {
    const double distance = (fallDistance * landing.distanceScale) + landing.extraDistance;
    const double power = distance + kEpsilon - safeFallDistance;
    return static_cast<std::int32_t>(
        std::floor(power * static_cast<double>(landing.damageMultiplier) * fallDamageMultiplier));
}

} // namespace basalt::server::runtime::world::fallLanding
