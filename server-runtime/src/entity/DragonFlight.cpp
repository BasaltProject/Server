#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::enderDragon {
namespace {

namespace mth = wire::common::mth;

constexpr float kRadiansPerDegree = static_cast<float>(std::numbers::pi / 180.0);
/// parity: `SITTING_ALLOWED_DAMAGE_PERCENTAGE`.
constexpr float kSittingAllowance = 0.25F;

[[nodiscard]] Vec3 normalize(const Vec3& v) {
    const double length = std::sqrt((v.x * v.x) + (v.y * v.y) + (v.z * v.z));
    if (length < static_cast<double>(1.0E-5F)) {
        return {};
    }
    return {v.x / length, v.y / length, v.z / length};
}

[[nodiscard]] double dot(const Vec3& a, const Vec3& b) { return (a.x * b.x) + (a.y * b.y) + (a.z * b.z); }

void place(const ServerEntity& dragon, DragonState& state, Part part, float x, double y, float z) {
    state.parts[static_cast<std::size_t>(part)] =
        Vec3{dragon.x + static_cast<double>(x), dragon.y + y, dragon.z + static_cast<double>(z)};
}

/// parity: `EnderDragon.getHeadYOffset`.
[[nodiscard]] float headYOffset(const DragonState& state) {
    if (isSitting(state.phase)) {
        return -1.0F;
    }
    return static_cast<float>(state.history.get(5).y - state.history.get(0).y);
}

/// parity: `AbstractDragonSittingPhase`.
[[nodiscard]] bool shrugsOffProjectiles(Phase phase) {
    return phase == Phase::SittingFlaming || phase == Phase::SittingScanning || phase == Phase::SittingAttacking;
}

} // namespace

DragonState freshState(const ServerEntity& dragon) {
    DragonState state;
    state.parts.fill(Vec3{dragon.x, dragon.y, dragon.z});
    return state;
}

void steer(ServerEntity& dragon, DragonState& state, DragonWorld& world) {
    dragon.yaw = wrapDegrees(dragon.yaw);
    if (dragon.noAi) {
        return;
    }
    state.history.record(dragon.y, dragon.yaw);
    // parity: a phase that hands over gets the new one's turn in the same tick.
    const Phase before = state.phase;
    tickPhase(dragon, state, world);
    if (state.phase != before) {
        tickPhase(dragon, state, world);
    }
    const std::optional<Vec3> target = flyTarget(state);
    if (!target.has_value()) {
        return;
    }
    const double xdd = target->x - dragon.x;
    double ydd = target->y - dragon.y;
    const double zdd = target->z - dragon.z;
    const double distToTarget = (xdd * xdd) + (ydd * ydd) + (zdd * zdd);
    const float max = flySpeed(state);
    const double horizontal = std::sqrt((xdd * xdd) + (zdd * zdd));
    if (horizontal > 0.0) {
        ydd = std::clamp(ydd / horizontal, static_cast<double>(-max), static_cast<double>(max));
    }
    dragon.velocityY += ydd * 0.01;
    dragon.yaw = wrapDegrees(dragon.yaw);
    const Vec3 aim = normalize({xdd, target->y - dragon.y, zdd});
    const Vec3 heading = normalize({static_cast<double>(mth::sin(dragon.yaw * kRadiansPerDegree)), dragon.velocityY,
                                    static_cast<double>(-mth::cos(dragon.yaw * kRadiansPerDegree))});
    const float facing = std::max((static_cast<float>(dot(heading, aim)) + 0.5F) / 1.5F, 0.0F);
    if (std::abs(xdd) > static_cast<double>(1.0E-5F) || std::abs(zdd) > static_cast<double>(1.0E-5F)) {
        const float wanted = std::clamp(
            wrapDegrees(180.0F -
                        (static_cast<float>(vanillaAtan2(xdd, zdd)) * (180.0F / static_cast<float>(std::numbers::pi))) -
                        dragon.yaw),
            -50.0F, 50.0F);
        state.yRotA *= 0.8F;
        state.yRotA += wanted * turnSpeed(dragon, state);
        dragon.yaw += state.yRotA * 0.1F;
    }
    const auto span = static_cast<float>(2.0 / (distToTarget + 1.0));
    // parity: `moveRelative(speed, (0, 0, -1))` turned by the heading.
    const float speed = 0.06F * ((facing * span) + (1.0F - span));
    const double forward = -1.0 * static_cast<double>(speed);
    const float sin = mth::sin(dragon.yaw * kRadiansPerDegree);
    const float cos = mth::cos(dragon.yaw * kRadiansPerDegree);
    dragon.velocityX += (0.0 * static_cast<double>(cos)) - (forward * static_cast<double>(sin));
    dragon.velocityZ += (forward * static_cast<double>(cos)) + (0.0 * static_cast<double>(sin));
    // parity: `noPhysics`, so the move is a straight shift that collides with nothing.
    const double scale = state.inWall ? static_cast<double>(0.8F) : 1.0;
    dragon.x += dragon.velocityX * scale;
    dragon.y += dragon.velocityY * scale;
    dragon.z += dragon.velocityZ * scale;
    const Vec3 actual = normalize({dragon.velocityX, dragon.velocityY, dragon.velocityZ});
    const double slide = 0.8 + (0.15 * (dot(actual, heading) + 1.0) / 2.0);
    dragon.velocityX *= slide;
    dragon.velocityY *= static_cast<double>(0.91F);
    dragon.velocityZ *= slide;
}

void placeBodyAndWings(const ServerEntity& dragon, DragonState& state) {
    const float rot = dragon.yaw * kRadiansPerDegree;
    const float sin = mth::sin(rot);
    const float cos = mth::cos(rot);
    place(dragon, state, Part::Body, sin * 0.5F, 0.0, -cos * 0.5F);
    place(dragon, state, Part::Wing1, cos * 4.5F, 2.0, sin * 4.5F);
    place(dragon, state, Part::Wing2, cos * -4.5F, 2.0, sin * -4.5F);
}

void placeRest(const ServerEntity& dragon, DragonState& state) {
    const float tilt = static_cast<float>(state.history.get(5).y - state.history.get(10).y) * 10.0F * kRadiansPerDegree;
    const float tiltCos = mth::cos(tilt);
    const float tiltSin = mth::sin(tilt);
    const float rot = dragon.yaw * kRadiansPerDegree;
    const float sin1 = mth::sin(rot);
    const float cos1 = mth::cos(rot);
    const float sin2 = mth::sin((dragon.yaw * kRadiansPerDegree) - (state.yRotA * 0.01F));
    const float cos2 = mth::cos((dragon.yaw * kRadiansPerDegree) - (state.yRotA * 0.01F));
    const float lift = headYOffset(state);
    place(dragon, state, Part::Head, sin2 * 6.5F * tiltCos, static_cast<double>(lift + (tiltSin * 6.5F)),
          -cos2 * 6.5F * tiltCos);
    place(dragon, state, Part::Neck, sin2 * 5.5F * tiltCos, static_cast<double>(lift + (tiltSin * 5.5F)),
          -cos2 * 5.5F * tiltCos);
    const FlightHistory::Sample& reference = state.history.get(5);
    for (std::int32_t i = 0; i < 3; ++i) {
        const FlightHistory::Sample& sample = state.history.get(12 + (i * 2));
        const float turn =
            (dragon.yaw * kRadiansPerDegree) +
            (static_cast<float>(wrapDegrees(static_cast<double>(sample.yRot - reference.yRot))) * kRadiansPerDegree);
        const float sin = mth::sin(turn);
        const float cos = mth::cos(turn);
        const auto reach = static_cast<float>(i + 1) * 2.0F;
        place(dragon, state, static_cast<Part>(static_cast<std::uint8_t>(Part::Tail1) + i),
              -((sin1 * 1.5F) + (sin * reach)) * tiltCos,
              sample.y - reference.y - static_cast<double>((reach + 1.5F) * tiltSin) + 1.5,
              ((cos1 * 1.5F) + (cos * reach)) * tiltCos);
    }
}

std::array<double, 6> partBox(const DragonState& state, Part part) {
    const Vec3& at = state.parts[static_cast<std::size_t>(part)];
    const PartBox& box = kPartBoxes[static_cast<std::size_t>(part)];
    const double half = static_cast<double>(box.width / 2.0F);
    return {at.x - half, at.y, at.z - half, at.x + half, at.y + static_cast<double>(box.height), at.z + half};
}

std::optional<float> damageFor(const DragonState& state, Part part, float damage, bool projectile) {
    if (state.phase == Phase::Dying) {
        return std::nullopt;
    }
    if (projectile && shrugsOffProjectiles(state.phase)) {
        damage = 0.0F;
    }
    if (part != Part::Head) {
        damage = (damage / 4.0F) + std::min(damage, 1.0F);
    }
    if (damage < 0.01F) {
        return std::nullopt;
    }
    return damage;
}

void afterHurt(DragonState& state, float healthBefore, float healthAfter) {
    if (!isSitting(state.phase)) {
        return;
    }
    state.sittingDamageReceived += healthBefore - healthAfter;
    if (state.sittingDamageReceived > kSittingAllowance * static_cast<float>(kMaxHealth)) {
        state.sittingDamageReceived = 0.0F;
        setPhase(state, Phase::Takeoff);
    }
}

bool refusesDeath(DragonState& state) {
    if (isSitting(state.phase)) {
        return false;
    }
    setPhase(state, Phase::Dying);
    return true;
}

} // namespace basalt::server::runtime::entity::enderDragon
