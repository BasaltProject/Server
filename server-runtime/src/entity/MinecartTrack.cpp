#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace basalt::server::runtime::entity::minecarts {
namespace {

using Vec = std::array<double, 3>;
using detail::Exits;
namespace mth = wire::common::mth;

constexpr double kEpsilon = static_cast<double>(1.0E-5F);
constexpr double kOnRailOffset = 0.1;
constexpr double kVShapeRest = 0.005;
constexpr double kSlopePush = static_cast<double>(0.0078125F);

[[nodiscard]] Vec add(const Vec& a, const Vec& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }

[[nodiscard]] Vec sub(const Vec& a, const Vec& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }

[[nodiscard]] Vec scale(const Vec& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }

[[nodiscard]] double dot(const Vec& a, const Vec& b) { return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]); }

[[nodiscard]] double lengthSqr(const Vec& a) { return dot(a, a); }

[[nodiscard]] double length(const Vec& a) { return std::sqrt(lengthSqr(a)); }

[[nodiscard]] double horizontalSqr(const Vec& a) { return (a[0] * a[0]) + (a[2] * a[2]); }

[[nodiscard]] double horizontalLength(const Vec& a) { return std::sqrt(horizontalSqr(a)); }

[[nodiscard]] Vec horizontalOf(const Vec& a) { return {a[0], 0.0, a[2]}; }

[[nodiscard]] double distance(const Vec& a, const Vec& b) { return length(sub(a, b)); }

[[nodiscard]] double distanceSqr(const Vec& a, const Vec& b) { return lengthSqr(sub(a, b)); }

/// parity: `Vec3.normalize`.
[[nodiscard]] Vec normalize(const Vec& a) {
    const double l = length(a);
    return l < 1.0E-5 ? Vec{} : Vec{a[0] / l, a[1] / l, a[2] / l};
}

[[nodiscard]] Vec toVec(const std::array<std::int32_t, 3>& a) {
    return {static_cast<double>(a[0]), static_cast<double>(a[1]), static_cast<double>(a[2])};
}

[[nodiscard]] Vec bottomCentre(const std::array<std::int32_t, 3>& pos) {
    return {pos[0] + 0.5, 1.0 * pos[1], pos[2] + 0.5};
}

[[nodiscard]] Vec positionOf(const ServerEntity& cart) { return {cart.x, cart.y, cart.z}; }

[[nodiscard]] Vec motionOf(const ServerEntity& cart) { return {cart.velocityX, cart.velocityY, cart.velocityZ}; }

void setPosition(ServerEntity& cart, const Vec& at) {
    cart.x = at[0];
    cart.y = at[1];
    cart.z = at[2];
}

void setMotion(ServerEntity& cart, const Vec& motion) {
    cart.velocityX = motion[0];
    cart.velocityY = motion[1];
    cart.velocityZ = motion[2];
}

[[nodiscard]] bool isSlope(std::string_view shape) { return shape.starts_with("ascending"); }

/// parity: `NewMinecartBehavior.isDecending`.
[[nodiscard]] bool descending(const Vec& movement, std::string_view shape) {
    if (shape == "ascending_east") {
        return movement[0] < 0.0;
    }
    if (shape == "ascending_west") {
        return movement[0] > 0.0;
    }
    if (shape == "ascending_north") {
        return movement[2] > 0.0;
    }
    if (shape == "ascending_south") {
        return movement[2] < 0.0;
    }
    return false;
}

/// parity: `NewMinecartBehavior.setRotation`, which flips a cart turned right round.
void setRotation(ServerEntity& cart, float yaw, float pitch) {
    CartState& state = stateOf(cart);
    const auto diff = static_cast<double>(std::abs(yaw - cart.yaw));
    if (diff >= 175.0 && diff <= 185.0) {
        state.flipped = !state.flipped;
        yaw -= 180.0F;
        pitch *= -1.0F;
    }
    pitch = std::clamp(pitch, -45.0F, 45.0F);
    cart.pitch = std::fmod(pitch, 360.0F);
    cart.yaw = std::fmod(yaw, 360.0F);
}

struct Iteration {
    double movementLeft = 0.0;
    bool first = true;
    bool gainedSlopeSpeed = false;
    bool halted = false;
    bool boosted = false;

    [[nodiscard]] bool shouldIterate() const { return first || movementLeft > kEpsilon; }
};

/// parity: the behaviour's own `getMaxSpeed`, without the furnace's share.
[[nodiscard]] double behaviourMaxSpeed(const ServerEntity& cart, const Env& env) {
    const bool inWater = cart.cart != nullptr && cart.cart->inWater;
    return static_cast<double>(env.maxMinecartSpeed) * (inWater ? 0.5 : 1.0) / 20.0;
}

[[nodiscard]] Vec slopeSpeed(const ServerEntity& cart, const Vec& movement, std::string_view shape) {
    double slide = std::max(kSlopePush, horizontalLength(movement) * 0.02);
    if (cart.cart != nullptr && cart.cart->inWater) {
        slide *= 0.2;
    }
    if (shape == "ascending_east") {
        return add(movement, {-slide, 0.0, 0.0});
    }
    if (shape == "ascending_west") {
        return add(movement, {slide, 0.0, 0.0});
    }
    if (shape == "ascending_north") {
        return add(movement, {0.0, 0.0, slide});
    }
    if (shape == "ascending_south") {
        return add(movement, {0.0, 0.0, -slide});
    }
    return movement;
}

[[nodiscard]] Vec playerInputSpeed(const Env& env, const Vec& movement) {
    if (!env.riderIntent.has_value() || lengthSqr(*env.riderIntent) <= 0.0) {
        return movement;
    }
    const Vec rider = normalize(*env.riderIntent);
    if (lengthSqr(rider) > 0.0 && horizontalSqr(movement) < 0.01) {
        return add(movement, scale(normalize({rider[0], 0.0, rider[2]}), 0.001));
    }
    return movement;
}

[[nodiscard]] Vec haltSpeed(const world::WorldState& world, const std::array<std::int32_t, 3>& pos,
                            const Vec& movement) {
    bool poweredRail = false;
    const bool powered = detail::poweredRail(world, pos[0], pos[1], pos[2], poweredRail);
    if (!poweredRail || powered) {
        return movement;
    }
    return length(movement) < 0.03 ? Vec{} : scale(movement, 0.5);
}

[[nodiscard]] Vec boostSpeed(const world::WorldState& world, const std::array<std::int32_t, 3>& pos,
                             std::string_view shape, const Vec& movement) {
    bool poweredRail = false;
    if (!detail::poweredRail(world, pos[0], pos[1], pos[2], poweredRail)) {
        return movement;
    }
    if (length(movement) > 0.01) {
        return scale(normalize(movement), length(movement) + 0.06);
    }
    // parity: `AbstractMinecart.getRedstoneDirection`, away from a conductor at either end.
    Vec direction{};
    if (shape == "east_west") {
        if (detail::isConductor(world, pos[0] - 1, pos[1], pos[2])) {
            direction = {1.0, 0.0, 0.0};
        } else if (detail::isConductor(world, pos[0] + 1, pos[1], pos[2])) {
            direction = {-1.0, 0.0, 0.0};
        }
    } else if (shape == "north_south") {
        if (detail::isConductor(world, pos[0], pos[1], pos[2] - 1)) {
            direction = {0.0, 0.0, 1.0};
        } else if (detail::isConductor(world, pos[0], pos[1], pos[2] + 1)) {
            direction = {0.0, 0.0, -1.0};
        }
    }
    return lengthSqr(direction) <= 0.0 ? movement : scale(direction, length(movement) + 0.2);
}

[[nodiscard]] Vec trackSpeed(world::WorldState& world, ServerEntity& cart, const Env& env, const Vec& movement,
                             Iteration& iteration, const std::array<std::int32_t, 3>& pos, std::string_view shape) {
    Vec speed = movement;
    if (!iteration.gainedSlopeSpeed) {
        const Vec sloped = slopeSpeed(cart, speed, shape);
        if (horizontalSqr(sloped) != horizontalSqr(speed)) {
            iteration.gainedSlopeSpeed = true;
            speed = sloped;
        }
    }
    if (iteration.first) {
        const Vec input = playerInputSpeed(env, speed);
        if (horizontalSqr(input) != horizontalSqr(speed)) {
            iteration.halted = true;
            speed = input;
        }
    }
    if (!iteration.halted) {
        const Vec halted = haltSpeed(world, pos, speed);
        if (horizontalSqr(halted) != horizontalSqr(speed)) {
            iteration.halted = true;
            speed = halted;
        }
    }
    if (iteration.first) {
        speed = detail::naturalSlowdown(cart, env, speed);
        if (lengthSqr(speed) > 0.0) {
            speed = scale(normalize(speed), std::min(length(speed), maxSpeed(cart, env)));
        }
    }
    if (!iteration.boosted) {
        const Vec boosted = boostSpeed(world, pos, shape, speed);
        if (horizontalSqr(boosted) != horizontalSqr(speed)) {
            iteration.boosted = true;
            speed = boosted;
        }
    }
    return speed;
}

/// parity: `NewMinecartBehavior.restAtVShape`.
[[nodiscard]] bool restAtVShape(ServerEntity& cart, std::string_view current, std::string_view next) {
    const Vec motion = motionOf(cart);
    if (lengthSqr(motion) < kVShapeRest && isSlope(next) && descending(motion, current) && !descending(motion, next)) {
        setMotion(cart, {});
        return true;
    }
    return false;
}

/// parity: `NewMinecartBehavior.stepAlongTrack`.
double stepAlongTrack(world::WorldState& world, ServerEntity& cart, const Env& env,
                      const std::array<std::int32_t, 3>& pos, std::string_view shape, double movementLeft) {
    if (movementLeft < kEpsilon) {
        return 0.0;
    }
    const Vec oldPosition = positionOf(cart);
    const Exits& exits = *detail::exitsOf(shape);
    Vec movement = horizontalOf(motionOf(cart));
    if (length(movement) < kEpsilon) {
        setMotion(cart, {});
        return 0.0;
    }
    const bool inHill = exits.a[1] != exits.b[1];
    const Vec in = horizontalOf(scale(toVec(exits.b), 0.5));
    Vec out = horizontalOf(scale(toVec(exits.a), 0.5));
    if (dot(movement, out) < dot(movement, in)) {
        out = in;
    }
    Vec outPosition = add(add(add(bottomCentre(pos), out), {0.0, kOnRailOffset, 0.0}), scale(normalize(out), kEpsilon));
    if (inHill && !descending(movement, shape)) {
        outPosition = add(outPosition, {0.0, 1.0, 0.0});
    }
    const Vec towardsOut = normalize(sub(outPosition, positionOf(cart)));
    movement = scale(towardsOut, length(movement) / horizontalLength(towardsOut));
    const double hill = inHill ? static_cast<double>(std::numbers::sqrt2_v<float>) : 1.0;
    Vec newPosition = add(oldPosition, scale(normalize(movement), movementLeft * hill));
    if (distanceSqr(oldPosition, outPosition) <= distanceSqr(oldPosition, newPosition)) {
        movementLeft = horizontalLength(sub(outPosition, newPosition));
        newPosition = outPosition;
    } else {
        movementLeft = 0.0;
    }
    const Vec delta = sub(newPosition, oldPosition);
    detail::moveAndPush(world, cart, env, delta[0], delta[1], delta[2]);
    if (inHill) {
        const std::optional<std::string_view> next =
            detail::shapeAt(world, mth::floor(newPosition[0]), mth::floor(newPosition[1]), mth::floor(newPosition[2]));
        if (next.has_value() && restAtVShape(cart, shape, *next)) {
            return 0.0;
        }
        const double fromOut = distance(horizontalOf(outPosition), horizontalOf(positionOf(cart)));
        const double projectedY = outPosition[1] + (descending(movement, shape) ? fromOut : -fromOut);
        if (cart.y < projectedY) {
            cart.y = projectedY;
        }
    }
    if (distance(positionOf(cart), oldPosition) < kEpsilon && distance(newPosition, oldPosition) > kEpsilon) {
        setMotion(cart, {});
        return 0.0;
    }
    setMotion(cart, movement);
    return movementLeft;
}

} // namespace

void adjustToRails(world::WorldState& world, ServerEntity& cart, const std::array<std::int32_t, 3>& rail,
                   bool instant) {
    const std::optional<std::string_view> shape = detail::shapeAt(world, rail[0], rail[1], rail[2]);
    if (!shape.has_value()) {
        return;
    }
    CartState& state = stateOf(cart);
    const Exits& exits = *detail::exitsOf(*shape);
    const Vec exit0 = scale(toVec(exits.a), 0.5);
    const Vec exit1 = scale(toVec(exits.b), 0.5);
    Vec out = horizontalOf(exit0);
    Vec in = horizontalOf(exit1);
    const Vec motion = motionOf(cart);
    if ((length(motion) > kEpsilon && dot(motion, out) < dot(motion, in)) || descending(in, *shape)) {
        std::swap(out, in);
    }
    float yaw = 180.0F - static_cast<float>(std::atan2(out[2], out[0]) * 180.0 / std::numbers::pi);
    yaw += state.flipped ? 180.0F : 0.0F;
    const Vec previous = positionOf(cart);
    const bool corner = exit0[0] != exit1[0] && exit0[2] != exit1[2];
    Vec target{};
    if (corner) {
        const Vec across = sub(exit1, exit0);
        const Vec fromExit = sub(sub(previous, bottomCentre(rail)), exit0);
        const Vec travel = scale(across, dot(across, fromExit) / dot(across, across));
        target = add(add(bottomCentre(rail), exit0), travel);
        yaw = 180.0F - static_cast<float>(std::atan2(travel[2], travel[0]) * 180.0 / std::numbers::pi);
        yaw += state.flipped ? 180.0F : 0.0F;
    } else {
        const bool zSnap = sub(exit0, exit1)[0] != 0.0;
        const bool xSnap = sub(exit0, exit1)[2] != 0.0;
        target = {xSnap ? rail[0] + 0.5 : previous[0], static_cast<double>(rail[1]),
                  zSnap ? rail[2] + 0.5 : previous[2]};
    }
    setPosition(cart, add(previous, sub(target, previous)));
    float pitch = 0.0F;
    if (exit0[1] != exit1[1]) {
        const Vec inPosition = add(bottomCentre(rail), in);
        const double fromIn = distance(inPosition, positionOf(cart));
        setPosition(cart, add(positionOf(cart), {0.0, fromIn + kOnRailOffset, 0.0}));
        pitch = state.flipped ? 45.0F : -45.0F;
    } else {
        setPosition(cart, add(positionOf(cart), {0.0, kOnRailOffset, 0.0}));
    }
    setRotation(cart, yaw, pitch);
    const double adjusted = distance(previous, positionOf(cart));
    if (adjusted > 0.0) {
        state.lerpSteps.push_back(Step{.position = positionOf(cart),
                                       .movement = motionOf(cart),
                                       .yaw = cart.yaw,
                                       .pitch = cart.pitch,
                                       .weight = instant ? 0.0F : static_cast<float>(adjusted)});
    }
}

namespace detail {

void newTick(world::WorldState& world, ServerEntity& cart, const Env& env) {
    CartState& state = stateOf(cart);
    if (state.firstTick) {
        const std::array<std::int32_t, 3> pos = currentBlockPosOrRailBelow(world, cart, true);
        state.onRails = isRail(world, pos[0], pos[1], pos[2]);
        adjustToRails(world, cart, pos, true);
    }
    cart.velocityY -= gravityOf(cart);
    // parity: the level's `setOldPosAndRot` before the turn, which the rails redo per step.
    Vec oldPosition = positionOf(cart);
    for (Iteration iteration; iteration.shouldIterate() && !cart.removed; iteration.first = false) {
        const Vec initial = motionOf(cart);
        const std::array<std::int32_t, 3> pos = currentBlockPosOrRailBelow(world, cart, true);
        const std::optional<std::string_view> shape = shapeAt(world, pos[0], pos[1], pos[2]);
        const bool onRails = shape.has_value();
        if (state.onRails != onRails) {
            state.onRails = onRails;
            adjustToRails(world, cart, pos, false);
        }
        if (onRails) {
            cart.fallDistance = 0.0;
            oldPosition = positionOf(cart);
            if (isActivator(world, pos[0], pos[1], pos[2]) && env.activate) {
                env.activate(cart, pos, poweredProperty(world, pos[0], pos[1], pos[2]));
            }
            const Vec speed = trackSpeed(world, cart, env, horizontalOf(initial), iteration, pos, *shape);
            if (iteration.first) {
                iteration.movementLeft = horizontalLength(speed);
            } else {
                iteration.movementLeft += horizontalLength(speed) - horizontalLength(initial);
            }
            setMotion(cart, speed);
            iteration.movementLeft = stepAlongTrack(world, cart, env, pos, *shape, iteration.movementLeft);
        } else {
            comeOffTrack(world, cart, env);
            iteration.movementLeft = 0.0;
        }
        const Vec step = sub(positionOf(cart), oldPosition);
        const double stepLength = length(step);
        if (stepLength > kEpsilon) {
            if (!(horizontalSqr(step) > kEpsilon)) {
                if (!state.onRails) {
                    // parity: `Mth.rotLerp(0.2F, xRot, 0.0F)`.
                    cart.pitch = cart.onGround ? 0.0F : cart.pitch + (0.2F * enderDragon::wrapDegrees(-cart.pitch));
                }
            } else {
                float yaw = 180.0F - static_cast<float>(std::atan2(step[2], step[0]) * 180.0 / std::numbers::pi);
                float pitch = cart.onGround && !state.onRails
                                  ? 0.0F
                                  : 90.0F - static_cast<float>(std::atan2(horizontalLength(step), step[1]) * 180.0 /
                                                               std::numbers::pi);
                yaw += state.flipped ? 180.0F : 0.0F;
                pitch *= state.flipped ? -1.0F : 1.0F;
                setRotation(cart, yaw, pitch);
            }
            state.lerpSteps.push_back(
                Step{.position = positionOf(cart),
                     .movement = motionOf(cart),
                     .yaw = cart.yaw,
                     .pitch = cart.pitch,
                     .weight = static_cast<float>(std::min(stepLength, behaviourMaxSpeed(cart, env)))});
        } else if (horizontalSqr(initial) > 0.0) {
            state.lerpSteps.push_back(Step{.position = positionOf(cart),
                                           .movement = motionOf(cart),
                                           .yaw = cart.yaw,
                                           .pitch = cart.pitch,
                                           .weight = 1.0F});
        }
        if ((stepLength > kEpsilon || iteration.first) && env.insideBlocks) {
            env.insideBlocks(cart);
            env.insideBlocks(cart);
        }
    }
}

} // namespace detail

} // namespace basalt::server::runtime::entity::minecarts
