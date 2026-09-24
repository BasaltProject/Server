#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ShieldBlows.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <string_view>

namespace basalt::server::runtime::entity::shieldBlows {
namespace {

namespace mth = wire::common::mth;

/// parity: `Ravager.blockedByItem`'s forty-tick stun, and the roar twenty ticks after it.
constexpr std::int32_t kStunTicks = 40;
constexpr std::int32_t kRoarTicks = 20;
constexpr std::int32_t kRoarAt = 10;
constexpr double kBaseSpeed = 0.3;
constexpr double kAttackSpeed = 0.35;
constexpr double kSpeedLerp = 0.1;

/// parity: `Entity.push(Entity)` from the defender's side: each gets a nudge apart, the defender's
/// answered, the ravager's applied.
[[nodiscard]] Push pushApart(ServerEntity& ravager, double defenderX, double defenderZ) {
    double xa = ravager.x - defenderX;
    double za = ravager.z - defenderZ;
    double distance = std::max(std::abs(xa), std::abs(za));
    if (distance < static_cast<double>(0.01F)) {
        return {};
    }
    distance = std::sqrt(distance);
    xa /= distance;
    za /= distance;
    const double power = std::min(1.0 / distance, 1.0);
    xa *= power * static_cast<double>(0.05F);
    za *= power * static_cast<double>(0.05F);
    ravager.velocityX += xa;
    ravager.velocityZ += za;
    return Push{.x = -xa, .y = 0.0, .z = -za};
}

/// parity: `HoglinBase.throwTarget`, which turns its shove by a raw number of radians.
[[nodiscard]] std::optional<Push> throwTarget(const ServerEntity& body, double targetX, double targetZ,
                                              double resistance, random::RandomSource& random) {
    const double power = body.attributeWithModifiers("minecraft:attack_knockback") - resistance;
    if (power <= 0.0) {
        return std::nullopt;
    }
    const double xd = targetX - body.x;
    const double zd = targetZ - body.z;
    const auto angle = static_cast<float>(random.nextInt(21) - 10);
    const double scale = power * static_cast<double>((random.nextFloat() * 0.5F) + 0.2F);
    const double length = std::sqrt((xd * xd) + (zd * zd));
    const double nx = length < 1.0E-5 ? 0.0 : xd / length * scale;
    const double nz = length < 1.0E-5 ? 0.0 : zd / length * scale;
    const auto cos = static_cast<double>(mth::cos(angle));
    const auto sin = static_cast<double>(mth::sin(angle));
    const double vertical = power * static_cast<double>(random.nextFloat()) * 0.5;
    return Push{.x = (nx * cos) + (nz * sin), .y = vertical, .z = (nz * cos) - (nx * sin)};
}

} // namespace

Push strongKnockback(const ServerEntity& ravager, double targetX, double targetZ) {
    const double xd = targetX - ravager.x;
    const double zd = targetZ - ravager.z;
    const double distance = std::max((xd * xd) + (zd * zd), 0.001);
    return Push{.x = xd / distance * 4.0, .y = 0.2, .z = zd / distance * 4.0};
}

Reply blocked(ServerEntity& attacker, double defenderX, double defenderZ, double defenderResistance,
              random::RandomSource& random) {
    const std::string_view path = attacker.type().path();
    if (path == "ravager") {
        if (attacker.ravagerRoarTicks != 0) {
            return {};
        }
        if (random.nextDouble() < 0.5) {
            attacker.ravagerStunTicks = kStunTicks;
            return Reply{.push = pushApart(attacker, defenderX, defenderZ), .stunned = true};
        }
        return Reply{.push = strongKnockback(attacker, defenderX, defenderZ)};
    }
    // parity: `Hoglin.blockedByItem` for an adult, `Zoglin`'s for anything but a baby.
    if ((path == "hoglin" || path == "zoglin") && attacker.age >= 0) {
        return Reply{.push = throwTarget(attacker, defenderX, defenderZ, defenderResistance, random)};
    }
    return {};
}

bool ravagerImmobile(const ServerEntity& ravager) {
    return ravager.ravagerAttackTicks > 0 || ravager.ravagerStunTicks > 0 || ravager.ravagerRoarTicks > 0;
}

RavagerTurn tickRavager(ServerEntity& ravager, bool hasTarget) {
    const double base = ravager.attribute(attribute::kMovementSpeed);
    ravager.attributeValues[std::string{attribute::kMovementSpeed}] =
        ravagerImmobile(ravager) ? 0.0 : base + (kSpeedLerp * ((hasTarget ? kAttackSpeed : kBaseSpeed) - base));
    RavagerTurn turn = RavagerTurn::Nothing;
    if (ravager.ravagerRoarTicks > 0 && --ravager.ravagerRoarTicks == kRoarAt) {
        turn = RavagerTurn::Roars;
    }
    if (ravager.ravagerAttackTicks > 0) {
        --ravager.ravagerAttackTicks;
    }
    if (ravager.ravagerStunTicks > 0 && --ravager.ravagerStunTicks == 0) {
        ravager.ravagerRoarTicks = kRoarTicks;
        turn = RavagerTurn::StunEnds;
    }
    return turn;
}

} // namespace basalt::server::runtime::entity::shieldBlows
