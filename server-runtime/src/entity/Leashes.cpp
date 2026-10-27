#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Leashes.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string_view>

namespace basalt::server::runtime::entity::leashes {
namespace {

using Vec = std::array<double, 3>;

/// parity: `AXIS_SPECIFIC_ELASTICITY` and `STIFFNESS`.
constexpr Vec kElasticity{0.8, 0.2, 0.8};
constexpr double kStiffness = 0.11;
/// parity: `computeDampenedSpringInteraction`'s damping when the pull goes the way it already moves.
constexpr double kDamping = static_cast<double>(0.3F);
constexpr double kTorsionalElasticity = 10.0;

/// parity: `Vec3.yRot`, which the attachment points turn with the body.
[[nodiscard]] Vec yRot(const Vec& vector, float radians) {
    const auto cos = static_cast<double>(wire::common::mth::cos(radians));
    const auto sin = static_cast<double>(wire::common::mth::sin(radians));
    return {(vector[0] * cos) + (vector[2] * sin), vector[1], (vector[2] * cos) - (vector[0] * sin)};
}

[[nodiscard]] double length(const Vec& vector) {
    return std::sqrt((vector[0] * vector[0]) + (vector[1] * vector[1]) + (vector[2] * vector[2]));
}

} // namespace

bool isLeashable(const ServerEntity& entity) {
    const std::string_view path = entity.type().path();
    return boats::isBoat(entity.type()) ||
           (registry::entityAttributes::isLiving(entity.type()) && path != "armor_stand" && path != "player");
}

bool canBeLeashed(const ServerEntity& entity, bool monster) {
    if (boats::isBoat(entity.type())) {
        return true;
    }
    const std::string_view path = entity.type().path();
    // parity: the overrides that refuse a leash: villagers, traders, pandas, turtles and water mobs.
    if (path == "villager" || path == "wandering_trader" || path == "panda" || path == "turtle" || path == "dolphin" ||
        path == "squid" || path == "glow_squid" || path == "cod" || path == "salmon" || path == "pufferfish" ||
        path == "tropical_fish" || path == "tadpole" || path == "axolotl") {
        return false;
    }
    // parity: the monsters that take a lead all the same.
    if (path == "hoglin" || path == "zoglin") {
        return true;
    }
    return !monster;
}

double snapDistance(const ServerEntity& entity, const double configured) {
    return entity.type().path() == "happy_ghast" ? 16.0 : configured;
}

double elasticDistance(const ServerEntity& entity) {
    return entity.type().path() == "happy_ghast" ? 10.0 : kElasticDistance;
}

double distance(const Body& entity, const Body& holder) {
    const Vec between{holder.x - entity.x, (holder.y + (holder.height / 2.0)) - (entity.y + (entity.height / 2.0)),
                      holder.z - entity.z};
    return length(between);
}

std::optional<Pull> elastic(const Body& entity, const Body& holder, double slack) {
    constexpr float kRadians = std::numbers::pi_v<float> / 180.0F;
    // parity: `ENTITY_ATTACHMENT_POINT` and `LEASHER_ATTACHMENT_POINT`.
    const Vec lever = yRot({0.0, 0.5 * entity.height, 0.5 * entity.width}, -(entity.yaw * kRadians));
    const Vec objectAt{entity.x + lever[0], entity.y + lever[1], entity.z + lever[2]};
    const Vec holderArm = yRot({0.0, 0.5 * holder.height, 0.0}, -(holder.yaw * kRadians));
    const Vec pivot{holder.x + holderArm[0], holder.y + holderArm[1], holder.z + holderArm[2]};
    const Vec between{pivot[0] - objectAt[0], pivot[1] - objectAt[1], pivot[2] - objectAt[2]};
    const double span = length(between);
    if (span < slack) {
        return std::nullopt;
    }
    Vec force{between[0] / span * (span - slack), between[1] / span * (span - slack),
              between[2] / span * (span - slack)};
    const double torque = (lever[2] * force[0]) - (lever[0] * force[2]);
    const Vec& moving = entity.movement;
    if ((moving[0] * force[0]) + (moving[1] * force[1]) + (moving[2] * force[2]) >= 0.0) {
        force = {force[0] * kDamping, force[1] * kDamping, force[2] * kDamping};
    }
    // parity: the entity is pushed along the rope and nudged towards moving with its holder.
    Pull pull{.angularMomentum = kTorsionalElasticity * torque};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        pull.push[axis] =
            (force[axis] * kElasticity[axis]) + ((holder.movement[axis] - entity.movement[axis]) * kStiffness);
    }
    return pull;
}

float angularFriction(bool onGround, float blockFriction, bool inLiquid) {
    if (onGround) {
        return blockFriction * 0.91F;
    }
    return inLiquid ? 0.8F : 0.91F;
}

} // namespace basalt::server::runtime::entity::leashes
