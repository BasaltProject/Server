#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockSupport.hpp>
#include <basalt/server/registry/World.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/DragonNavigation.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/StuckSpeed.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::minecarts {
namespace {

namespace mth = wire::common::mth;
using detail::Exits;

/// parity: the 0.75 a cart with anybody aboard moves its own speed by.
constexpr double kVehicleScale = 0.75;
/// parity: the nudge a rider leaning on a key gives a cart that is barely moving.
constexpr double kRiderPush = 0.001;
constexpr double kRiderPushLimit = 0.01;
/// parity: the `slideSpeed` a slope is worth.
constexpr double kSlopePush = static_cast<double>(0.0078125F);
/// parity: what a powered rail adds, and the nudge it gives a cart standing still.
constexpr double kBoost = 0.06;
constexpr double kNudge = 0.02;
/// parity: `getPos`'s sixteenth of a block, and the twentieth of the drop it turns into speed.
constexpr double kRailLift = static_cast<double>(0.0625F);
constexpr double kSlopeSpeed = 0.05;
/// parity: `comeOffTrack`: half the speed on the ground, and `getAirDrag`'s 0.95 in the air.
constexpr double kGroundDrag = 0.5;
constexpr double kAirDrag = static_cast<double>(0.95F);
/// parity: under this, an unpowered powered rail stops the cart dead.
constexpr double kHaltSpeed = 0.03;
/// parity: `getDefaultGravity`, and the water's own slowdown.
constexpr double kGravity = 0.04;
constexpr double kWaterGravity = 0.005;
constexpr double kWaterSlowdown = static_cast<double>(0.95F);
/// parity: `getSlowdownFactor`, a cart with somebody in it keeping far more of its speed.
constexpr double kSlowdown = 0.96;
constexpr double kVehicleSlowdown = 0.997;
constexpr double kWidth = static_cast<double>(0.98F);
constexpr double kHeight = static_cast<double>(0.7F);

const std::map<std::string, Exits, std::less<>>& exitTable() {
    static const std::map<std::string, Exits, std::less<>> table = {
        {"north_south", {{0, 0, -1}, {0, 0, 1}}},      {"east_west", {{-1, 0, 0}, {1, 0, 0}}},
        {"ascending_east", {{-1, -1, 0}, {1, 0, 0}}},  {"ascending_west", {{-1, 0, 0}, {1, -1, 0}}},
        {"ascending_north", {{0, 0, -1}, {0, -1, 1}}}, {"ascending_south", {{0, -1, -1}, {0, 0, 1}}},
        {"south_east", {{0, 0, 1}, {1, 0, 0}}},        {"south_west", {{0, 0, 1}, {-1, 0, 0}}},
        {"north_west", {{0, 0, -1}, {-1, 0, 0}}},      {"north_east", {{0, 0, -1}, {1, 0, 0}}},
    };
    return table;
}

[[nodiscard]] bool isRailPath(std::string_view path) {
    return path == "rail" || path == "powered_rail" || path == "detector_rail" || path == "activator_rail";
}

[[nodiscard]] double horizontal(double x, double z) { return std::sqrt((x * x) + (z * z)); }

/// parity: `Mth.equal`, within a float's hundred-thousandth.
[[nodiscard]] bool nearlyEqual(double a, double b) { return std::abs(b - a) < static_cast<double>(1.0E-5F); }

/// parity: `OldMinecartBehavior.moveAlongTrack`.
void oldMoveAlongTrack(world::WorldState& world, ServerEntity& cart, const Env& env) {
    CartState& state = stateOf(cart);
    const std::array<std::int32_t, 3> pos = currentBlockPosOrRailBelow(world, cart, false);
    const std::optional<std::string_view> shape = detail::shapeAt(world, pos[0], pos[1], pos[2]);
    const Exits* exits = shape.has_value() ? detail::exitsOf(*shape) : nullptr;
    if (exits == nullptr) {
        return;
    }
    cart.fallDistance = 0.0;
    double x = cart.x;
    double y = cart.y;
    double z = cart.z;
    const std::optional<std::array<double, 3>> oldPos = railPosition(world, x, y, z);
    y = pos[1];
    bool isPoweredRail = false;
    const bool powered = detail::poweredRail(world, pos[0], pos[1], pos[2], isPoweredRail);
    bool halting = isPoweredRail && !powered;
    double slide = kSlopePush;
    if (state.inWater) {
        slide *= 0.2;
    }
    if (*shape == "ascending_east") {
        cart.velocityX -= slide;
        ++y;
    } else if (*shape == "ascending_west") {
        cart.velocityX += slide;
        ++y;
    } else if (*shape == "ascending_north") {
        cart.velocityZ += slide;
        ++y;
    } else if (*shape == "ascending_south") {
        cart.velocityZ -= slide;
        ++y;
    }
    // parity: the speed is projected onto the rail, keeping its size and taking its direction.
    double xD = exits->b[0] - exits->a[0];
    double zD = exits->b[2] - exits->a[2];
    const double length = std::sqrt((xD * xD) + (zD * zD));
    if ((cart.velocityX * xD) + (cart.velocityZ * zD) < 0.0) {
        xD = -xD;
        zD = -zD;
    }
    const double pow = std::min(2.0, horizontal(cart.velocityX, cart.velocityZ));
    cart.velocityX = pow * xD / length;
    cart.velocityZ = pow * zD / length;
    // parity: a player leaning on a key gets a stopped cart going, which also lifts the brake.
    if (env.riderIntent.has_value()) {
        const std::array<double, 3>& intent = *env.riderIntent;
        const double intentLength = (intent[0] * intent[0]) + (intent[1] * intent[1]) + (intent[2] * intent[2]);
        if (intentLength > 0.0 &&
            (cart.velocityX * cart.velocityX) + (cart.velocityZ * cart.velocityZ) < kRiderPushLimit) {
            cart.velocityX += intent[0] * kRiderPush;
            cart.velocityZ += intent[2] * kRiderPush;
            halting = false;
        }
    }
    if (halting) {
        if (horizontal(cart.velocityX, cart.velocityZ) < kHaltSpeed) {
            cart.velocityX = 0.0;
            cart.velocityY = 0.0;
            cart.velocityZ = 0.0;
        } else {
            cart.velocityX *= 0.5;
            cart.velocityY = 0.0;
            cart.velocityZ *= 0.5;
        }
    }
    // parity: the cart is pinned to the line between the two exits.
    const double x0 = pos[0] + 0.5 + (exits->a[0] * 0.5);
    const double z0 = pos[2] + 0.5 + (exits->a[2] * 0.5);
    const double x1 = pos[0] + 0.5 + (exits->b[0] * 0.5);
    const double z1 = pos[2] + 0.5 + (exits->b[2] * 0.5);
    xD = x1 - x0;
    zD = z1 - z0;
    double progress = 0.0;
    if (xD == 0.0) {
        progress = z - pos[2];
    } else if (zD == 0.0) {
        progress = x - pos[0];
    } else {
        progress = (((x - x0) * xD) + ((z - z0) * zD)) * 2.0;
    }
    cart.x = x0 + (xD * progress);
    cart.y = y;
    cart.z = z0 + (zD * progress);
    const double scale = cart.passengerIds.empty() ? 1.0 : kVehicleScale;
    const double limit = maxSpeed(cart, env);
    move(world, cart, env, std::clamp(scale * cart.velocityX, -limit, limit), 0.0,
         std::clamp(scale * cart.velocityZ, -limit, limit));
    const std::int32_t steppedX = mth::floor(cart.x) - pos[0];
    const std::int32_t steppedZ = mth::floor(cart.z) - pos[2];
    if (exits->a[1] != 0 && steppedX == exits->a[0] && steppedZ == exits->a[2]) {
        cart.y += exits->a[1];
    } else if (exits->b[1] != 0 && steppedX == exits->b[0] && steppedZ == exits->b[2]) {
        cart.y += exits->b[1];
    }
    const std::array<double, 3> slowed =
        detail::naturalSlowdown(cart, env, {cart.velocityX, cart.velocityY, cart.velocityZ});
    cart.velocityX = slowed[0];
    cart.velocityY = slowed[1];
    cart.velocityZ = slowed[2];
    // parity: the twentieth of the height it lost, added to its speed.
    const std::optional<std::array<double, 3>> newPos = railPosition(world, cart.x, cart.y, cart.z);
    if (newPos.has_value() && oldPos.has_value()) {
        const double gained = ((*oldPos)[1] - (*newPos)[1]) * kSlopeSpeed;
        const double moving = horizontal(cart.velocityX, cart.velocityZ);
        if (moving > 0.0) {
            cart.velocityX *= (moving + gained) / moving;
            cart.velocityZ *= (moving + gained) / moving;
        }
        cart.y = (*newPos)[1];
    }
    // parity: a cart that crossed into the next block points its speed the way it stepped.
    const std::int32_t xn = mth::floor(cart.x);
    const std::int32_t zn = mth::floor(cart.z);
    if (xn != pos[0] || zn != pos[2]) {
        const double moving = horizontal(cart.velocityX, cart.velocityZ);
        cart.velocityX = moving * (xn - pos[0]);
        cart.velocityZ = moving * (zn - pos[2]);
    }
    if (!powered) {
        return;
    }
    const double moving = horizontal(cart.velocityX, cart.velocityZ);
    if (moving > 0.01) {
        cart.velocityX += cart.velocityX / moving * kBoost;
        cart.velocityZ += cart.velocityZ / moving * kBoost;
        return;
    }
    if (*shape == "east_west") {
        if (detail::isConductor(world, pos[0] - 1, pos[1], pos[2])) {
            cart.velocityX = kNudge;
        } else if (detail::isConductor(world, pos[0] + 1, pos[1], pos[2])) {
            cart.velocityX = -kNudge;
        }
    } else if (*shape == "north_south") {
        if (detail::isConductor(world, pos[0], pos[1], pos[2] - 1)) {
            cart.velocityZ = kNudge;
        } else if (detail::isConductor(world, pos[0], pos[1], pos[2] + 1)) {
            cart.velocityZ = -kNudge;
        }
    }
}

/// parity: `OldMinecartBehavior.tick` on the server.
void oldTick(world::WorldState& world, ServerEntity& cart, const Env& env, const std::array<double, 3>& oldPos,
             float oldYaw) {
    CartState& state = stateOf(cart);
    cart.velocityY -= detail::gravityOf(cart);
    const std::array<std::int32_t, 3> pos = currentBlockPosOrRailBelow(world, cart, false);
    const bool onRails = isRail(world, pos[0], pos[1], pos[2]);
    state.onRails = onRails;
    if (onRails) {
        oldMoveAlongTrack(world, cart, env);
        if (detail::isActivator(world, pos[0], pos[1], pos[2]) && env.activate) {
            env.activate(cart, pos, detail::poweredProperty(world, pos[0], pos[1], pos[2]));
        }
    } else {
        detail::comeOffTrack(world, cart, env);
    }
    if (env.insideBlocks) {
        env.insideBlocks(cart);
    }
    cart.pitch = 0.0F;
    const double xDiff = oldPos[0] - cart.x;
    const double zDiff = oldPos[2] - cart.z;
    if ((xDiff * xDiff) + (zDiff * zDiff) > 0.001) {
        cart.yaw = static_cast<float>(std::atan2(zDiff, xDiff) * 180.0 / std::numbers::pi);
        if (state.flipped) {
            cart.yaw += 180.0F;
        }
    }
    const double rotDiff = static_cast<double>(enderDragon::wrapDegrees(cart.yaw - oldYaw));
    if (rotDiff < -170.0 || rotDiff >= 170.0) {
        cart.yaw += 180.0F;
        state.flipped = !state.flipped;
    }
    cart.pitch = std::fmod(cart.pitch, 360.0F);
    cart.yaw = std::fmod(cart.yaw, 360.0F);
    if (env.pushAndPickup) {
        static_cast<void>(env.pushAndPickup(cart));
    }
}

} // namespace

namespace detail {

std::optional<std::string_view> shapeAt(const world::WorldState& world, std::int32_t x, std::int32_t y,
                                        std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || !isRailPath(block->path())) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto shape = properties.find("shape");
    if (shape == properties.end()) {
        return std::nullopt;
    }
    const auto found = exitTable().find(shape->second);
    return found == exitTable().end() ? std::nullopt : std::optional<std::string_view>{found->first};
}

const Exits* exitsOf(std::string_view shape) {
    const auto found = exitTable().find(shape);
    return found == exitTable().end() ? nullptr : &found->second;
}

bool poweredRail(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z, bool& isPoweredRail) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    isPoweredRail = block != nullptr && block->path() == "powered_rail";
    if (!isPoweredRail) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto powered = properties.find("powered");
    return powered != properties.end() && powered->second == "true";
}

bool isActivator(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    return block != nullptr && block->path() == "activator_rail";
}

bool poweredProperty(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto powered = properties.find("powered");
    return powered != properties.end() && powered->second == "true";
}

void moveAndPush(world::WorldState& world, ServerEntity& cart, const Env& env, double dx, double dy, double dz) {
    if (!env.experimental) {
        move(world, cart, env, dx, dy, dz);
        return;
    }
    const std::array<double, 3> target{cart.x + dx, cart.y + dy, cart.z + dz};
    move(world, cart, env, dx, dy, dz);
    if (env.pushAndPickup && env.pushAndPickup(cart)) {
        move(world, cart, env, target[0] - cart.x, target[1] - cart.y, target[2] - cart.z);
    }
}

bool isConductor(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    // parity: `BlockState.isRedstoneConductor`, which is a full solid cube for nearly every block.
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && registry::blockSupport::conductsRedstone(*stateId);
}

double gravityOf(const ServerEntity& cart) {
    return cart.cart != nullptr && cart.cart->inWater ? kWaterGravity : kGravity;
}

std::array<double, 3> naturalSlowdown(ServerEntity& cart, const Env& env, const std::array<double, 3>& movement) {
    CartState& state = stateOf(cart);
    std::array<double, 3> base = movement;
    // parity: `MinecartFurnace.applyNaturalSlowdown`, the push along the rails before the usual one.
    if (isFurnace(cart)) {
        if ((state.pushX * state.pushX) + (state.pushZ * state.pushZ) > 1.0E-7) {
            const double pushLength = horizontal(state.pushX, state.pushZ);
            if ((state.pushX * state.pushX) + (state.pushZ * state.pushZ) > 1.0E-4 &&
                (movement[0] * movement[0]) + (movement[2] * movement[2]) > 0.001) {
                // parity: `projectedOn(deltaMovement).normalize().scale(push.length())`.
                const double lengthSqr =
                    (movement[0] * movement[0]) + (movement[1] * movement[1]) + (movement[2] * movement[2]);
                const double along = ((state.pushX * movement[0]) + (state.pushZ * movement[2])) / lengthSqr;
                double px = movement[0] * along;
                double py = movement[1] * along;
                double pz = movement[2] * along;
                const double projected = std::sqrt((px * px) + (py * py) + (pz * pz));
                if (projected < 1.0E-5) {
                    px = 0.0;
                    pz = 0.0;
                } else {
                    px = px / projected * pushLength;
                    pz = pz / projected * pushLength;
                }
                state.pushX = px;
                state.pushZ = pz;
            }
            base = {(movement[0] * 0.8) + state.pushX, 0.0, (movement[2] * 0.8) + state.pushZ};
            if (state.inWater) {
                base = {base[0] * 0.1, base[1] * 0.1, base[2] * 0.1};
            }
        } else {
            base = {movement[0] * 0.98, 0.0, movement[2] * 0.98};
        }
    }
    const bool vehicle = !cart.passengerIds.empty();
    const double factor =
        env.experimental ? (vehicle ? kVehicleSlowdown : 0.975) : (vehicle ? kVehicleSlowdown : kSlowdown);
    std::array<double, 3> slowed{base[0] * factor, 0.0, base[2] * factor};
    if (state.inWater) {
        slowed = {slowed[0] * kWaterSlowdown, slowed[1] * kWaterSlowdown, slowed[2] * kWaterSlowdown};
    }
    return slowed;
}

void comeOffTrack(world::WorldState& world, ServerEntity& cart, const Env& env) {
    const double limit = maxSpeed(cart, env);
    cart.velocityX = std::clamp(cart.velocityX, -limit, limit);
    cart.velocityZ = std::clamp(cart.velocityZ, -limit, limit);
    if (cart.onGround) {
        cart.velocityX *= kGroundDrag;
        cart.velocityY *= kGroundDrag;
        cart.velocityZ *= kGroundDrag;
    }
    moveAndPush(world, cart, env, cart.velocityX, cart.velocityY, cart.velocityZ);
    if (!cart.onGround) {
        cart.velocityX *= kAirDrag;
        cart.velocityY *= kAirDrag;
        cart.velocityZ *= kAirDrag;
    }
}

} // namespace detail

CartState& stateOf(ServerEntity& cart) {
    if (cart.cart == nullptr) {
        cart.cart = std::make_shared<CartState>();
    }
    return *cart.cart;
}

bool isMinecart(const wire::common::Identifier& type) {
    const std::string_view path = type.path();
    return path == "minecart" || path.ends_with("_minecart");
}

bool isRideable(const ServerEntity& cart) { return cart.type().path() == "minecart"; }

bool isFurnace(const ServerEntity& cart) { return cart.type().path() == "furnace_minecart"; }

bool isRail(const world::WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    return block != nullptr && isRailPath(block->path());
}

std::array<std::int32_t, 3> currentBlockPosOrRailBelow(const world::WorldState& world, const ServerEntity& cart,
                                                       bool experimental) {
    const std::int32_t x = mth::floor(cart.x);
    std::int32_t y = mth::floor(cart.y);
    const std::int32_t z = mth::floor(cart.z);
    if (experimental) {
        const double below = cart.y - 0.1 - static_cast<double>(1.0E-5F);
        if (isRail(world, x, mth::floor(below), z)) {
            y = mth::floor(below);
        }
    } else if (isRail(world, x, y - 1, z)) {
        --y;
    }
    return {x, y, z};
}

std::optional<std::array<std::int32_t, 3>> railUnder(const world::WorldState& world, const ServerEntity& cart,
                                                     bool experimental) {
    const std::array<std::int32_t, 3> pos = currentBlockPosOrRailBelow(world, cart, experimental);
    return isRail(world, pos[0], pos[1], pos[2]) ? std::optional{pos} : std::nullopt;
}

std::optional<std::array<double, 3>> railPosition(const world::WorldState& world, double x, double y, double z) {
    const std::int32_t xt = mth::floor(x);
    std::int32_t yt = mth::floor(y);
    const std::int32_t zt = mth::floor(z);
    if (isRail(world, xt, yt - 1, zt)) {
        --yt;
    }
    const std::optional<std::string_view> shape = detail::shapeAt(world, xt, yt, zt);
    const Exits* exits = shape.has_value() ? detail::exitsOf(*shape) : nullptr;
    if (exits == nullptr) {
        return std::nullopt;
    }
    const double x0 = xt + 0.5 + (exits->a[0] * 0.5);
    const double y0 = yt + kRailLift + (exits->a[1] * 0.5);
    const double z0 = zt + 0.5 + (exits->a[2] * 0.5);
    const double xD = xt + 0.5 + (exits->b[0] * 0.5) - x0;
    const double yD = (yt + kRailLift + (exits->b[1] * 0.5) - y0) * 2.0;
    const double zD = zt + 0.5 + (exits->b[2] * 0.5) - z0;
    double progress = 0.0;
    if (xD == 0.0) {
        progress = z - zt;
    } else if (zD == 0.0) {
        progress = x - xt;
    } else {
        progress = (((x - x0) * xD) + ((z - z0) * zD)) * 2.0;
    }
    double height = y0 + (yD * progress);
    if (yD < 0.0) {
        height += 1.0;
    } else if (yD > 0.0) {
        height += 0.5;
    }
    return std::array<double, 3>{x0 + (xD * progress), height, z0 + (zD * progress)};
}

double maxSpeed(const ServerEntity& cart, const Env& env) {
    const bool inWater = cart.cart != nullptr && cart.cart->inWater;
    double speed = env.experimental ? static_cast<double>(env.maxMinecartSpeed) * (inWater ? 0.5 : 1.0) / 20.0
                                    : (inWater ? kMaxSpeedInWater : kMaxSpeed);
    // parity: `MinecartFurnace.getMaxSpeed`, three quarters of it in water and half on land.
    if (isFurnace(cart)) {
        speed *= inWater ? static_cast<double>(0.75F) : 0.5;
    }
    return speed;
}

void tick(world::WorldState& world, ServerEntity& cart, const Env& env) {
    const std::array<double, 3> oldPos{cart.x, cart.y, cart.z};
    const float oldYaw = cart.yaw;
    if (env.experimental) {
        detail::newTick(world, cart, env);
        return;
    }
    oldTick(world, cart, env, oldPos, oldYaw);
}

void move(world::WorldState& world, ServerEntity& cart, const Env& env, double dx, double dy, double dz) {
    CartState& state = stateOf(cart);
    // parity: `Entity.move`: a cobweb's hold is spent on this move and takes the speed with it.
    if ((state.stuck[0] * state.stuck[0]) + (state.stuck[1] * state.stuck[1]) + (state.stuck[2] * state.stuck[2]) >
        1.0E-7) {
        dx *= state.stuck[0];
        dy *= state.stuck[1];
        dz *= state.stuck[2];
        state.stuck = {};
        cart.velocityX = 0.0;
        cart.velocityY = 0.0;
        cart.velocityZ = 0.0;
    }
    const entityCollision::BlockAt blockAt = [&world](std::int32_t x, std::int32_t y, std::int32_t z) {
        return world.blockState(x, y, z);
    };
    const Aabb box = Aabb::of(cart.x, cart.y, cart.z, kWidth, kHeight);
    const std::array<double, 3> moved = entityCollision::collide(blockAt, box, dx, dy, dz);
    const double movedLength = (moved[0] * moved[0]) + (moved[1] * moved[1]) + (moved[2] * moved[2]);
    const double wantedLength = (dx * dx) + (dy * dy) + (dz * dz);
    if (movedLength > 1.0E-7 || wantedLength - movedLength < 1.0E-7) {
        cart.x += moved[0];
        cart.y += moved[1];
        cart.z += moved[2];
    }
    const bool xCollision = !nearlyEqual(dx, moved[0]);
    const bool zCollision = !nearlyEqual(dz, moved[2]);
    state.horizontalCollision = xCollision || zCollision;
    cart.horizontallyBlocked = state.horizontalCollision;
    state.verticalCollision = dy != moved[1];
    const bool below = state.verticalCollision && dy < 0.0;
    cart.onGround = below;
    // parity: `checkFallDamage`, out of water the fall counts, and landing hands it to the block.
    const std::int32_t effectX = mth::floor(cart.x);
    const std::int32_t effectY = mth::floor(cart.y - 0.2);
    const std::int32_t effectZ = mth::floor(cart.z);
    const std::int32_t effectState = world.blockState(effectX, effectY, effectZ).value_or(0);
    if (!state.inWater && moved[1] < 0.0) {
        cart.fallDistance -= static_cast<double>(static_cast<float>(moved[1]));
    }
    if (cart.onGround) {
        if (cart.fallDistance > 0.0 && env.landed) {
            env.landed(cart, cart.fallDistance);
        }
        cart.fallDistance = 0.0;
    }
    if (cart.removed) {
        return;
    }
    // parity: `restituteMovementAfterCollisions`: nothing bounces off a wall, and the floor gives
    // back only what a bouncy block below lends a thing that is not alive.
    if ((dy != 0.0 && state.verticalCollision) || state.horizontalCollision) {
        if (xCollision) {
            cart.velocityX = -cart.velocityX * 0.0;
        }
        if (zCollision) {
            cart.velocityZ = -cart.velocityZ * 0.0;
        }
        if (state.verticalCollision) {
            double restitution = 0.0;
            if (below && -cart.velocityY > detail::gravityOf(cart)) {
                restitution = static_cast<double>(registry::blockMovement::bouncinessOf(effectState) * 0.8F);
            }
            double compensation = 0.0;
            double drag = 1.0;
            if (restitution > 0.0 && cart.velocityY != 0.0) {
                const double portion = moved[1] / cart.velocityY;
                compensation = portion * detail::gravityOf(cart);
                drag = 1.0 + (portion * (kAirDrag - 1.0));
            }
            cart.velocityY = (compensation - cart.velocityY) * drag * restitution;
        }
    }
    // parity: `AbstractMinecart.getBlockSpeedFactor`, whole on a rail and the block's own elsewhere.
    const std::optional<std::int32_t> here =
        world.blockState(mth::floor(cart.x), mth::floor(cart.y), mth::floor(cart.z));
    if (!isRail(world, mth::floor(cart.x), mth::floor(cart.y), mth::floor(cart.z))) {
        float factor = here.has_value() ? registry::blockMovement::speedFactorOf(*here) : 1.0F;
        const wire::common::Identifier* hereBlock =
            here.has_value() ? registry::blockStateDefinitions::blockOf(*here) : nullptr;
        const bool watery =
            hereBlock != nullptr && (hereBlock->path() == "water" || hereBlock->path() == "bubble_column");
        if (factor == 1.0F && !watery) {
            factor = registry::blockMovement::speedFactorOf(
                world.blockState(mth::floor(cart.x), mth::floor(cart.y - 0.500001), mth::floor(cart.z)).value_or(0));
        }
        cart.velocityX *= static_cast<double>(factor);
        cart.velocityZ *= static_cast<double>(factor);
    }
    // parity: the old behaviour's own `applyEffectsFromBlocks` after every move.
    if (!env.experimental && env.insideBlocks) {
        env.insideBlocks(cart);
    }
}

std::array<double, 3> intentOf(bool forward, bool back, bool left, bool right, float yaw) {
    const float sideways = left == right ? 0.0F : (left ? 1.0F : -1.0F);
    const float ahead = forward == back ? 0.0F : (forward ? 1.0F : -1.0F);
    double x = static_cast<double>(sideways);
    double z = static_cast<double>(ahead);
    const double lengthSqr = (x * x) + (z * z);
    if (lengthSqr < 1.0E-7) {
        return {0.0, 0.0, 0.0};
    }
    if (lengthSqr > 1.0) {
        const double length = std::sqrt(lengthSqr);
        x /= length;
        z /= length;
    }
    const float radians = yaw * (std::numbers::pi_v<float> / 180.0F);
    const auto sin = static_cast<double>(mth::sin(radians));
    const auto cos = static_cast<double>(mth::cos(radians));
    return {(x * cos) - (z * sin), 0.0, (z * cos) + (x * sin)};
}

void pushOtherMinecart(ServerEntity& self, ServerEntity& other, double xa, double za, bool experimental) {
    double xo = 0.0;
    double zo = 0.0;
    if (experimental) {
        xo = self.velocityX;
        zo = self.velocityZ;
    } else {
        xo = other.x - self.x;
        zo = other.z - self.z;
    }
    const double dirLength = horizontal(xo, zo);
    const double dirX = dirLength < 1.0E-5 ? 0.0 : xo / dirLength;
    const double dirZ = dirLength < 1.0E-5 ? 0.0 : zo / dirLength;
    const float radians = self.yaw * (std::numbers::pi_v<float> / 180.0F);
    const auto facingX = static_cast<double>(mth::cos(radians));
    const auto facingZ = static_cast<double>(mth::sin(radians));
    const double facingLength = horizontal(facingX, facingZ);
    const double dot = std::abs(((dirX * facingX) + (dirZ * facingZ)) / (facingLength < 1.0E-5 ? 1.0 : facingLength));
    if (dot < static_cast<double>(0.8F) && !experimental) {
        return;
    }
    const std::array<double, 2> mine{self.velocityX, self.velocityZ};
    const std::array<double, 2> theirs{other.velocityX, other.velocityZ};
    if (isFurnace(other) && !isFurnace(self)) {
        self.velocityX = (mine[0] * 0.2) + (theirs[0] - xa);
        self.velocityZ = (mine[1] * 0.2) + (theirs[1] - za);
        other.velocityX = theirs[0] * 0.95;
        other.velocityZ = theirs[1] * 0.95;
    } else if (!isFurnace(other) && isFurnace(self)) {
        other.velocityX = (theirs[0] * 0.2) + (mine[0] + xa);
        other.velocityZ = (theirs[1] * 0.2) + (mine[1] + za);
        self.velocityX = mine[0] * 0.95;
        self.velocityZ = mine[1] * 0.95;
    } else {
        const double xdd = (theirs[0] + mine[0]) / 2.0;
        const double zdd = (theirs[1] + mine[1]) / 2.0;
        self.velocityX = (mine[0] * 0.2) + (xdd - xa);
        self.velocityZ = (mine[1] * 0.2) + (zdd - za);
        other.velocityX = (theirs[0] * 0.2) + (xdd + xa);
        other.velocityZ = (theirs[1] * 0.2) + (zdd + za);
    }
}

bool addFuel(ServerEntity& cart, double fromX, double fromZ) {
    CartState& state = stateOf(cart);
    if (state.fuel + kFuelPerItem > kMaxFuel) {
        return false;
    }
    state.fuel += kFuelPerItem;
    state.pushX = cart.x - fromX;
    state.pushZ = cart.z - fromZ;
    return true;
}

void save(const ServerEntity& cart, wire::nbt::NbtCompound& tag) {
    const CartState fresh;
    const CartState& state = cart.cart != nullptr ? *cart.cart : fresh;
    tag.putBoolean("FlippedRotation", state.flipped);
    // parity: vanilla writes its `firstTick` under this name.
    tag.putBoolean("HasTicked", state.firstTick);
    if (isFurnace(cart)) {
        tag.putDouble("PushX", state.pushX);
        tag.putDouble("PushZ", state.pushZ);
        tag.putShort("Fuel", static_cast<std::int16_t>(state.fuel));
    }
    if (cart.type().path() == "tnt_minecart") {
        if (state.explosionPower != kTntCartPower) {
            tag.putFloat("explosion_power", state.explosionPower);
        }
        if (state.explosionSpeedFactor != kTntCartSpeedFactor) {
            tag.putFloat("explosion_speed_factor", state.explosionSpeedFactor);
        }
    }
}

void load(ServerEntity& cart, const wire::nbt::NbtCompound& tag) {
    CartState& state = stateOf(cart);
    state.flipped = tag.getBoolean("FlippedRotation", false);
    state.firstTick = tag.getBoolean("HasTicked", false);
    if (isFurnace(cart)) {
        state.pushX = tag.getDouble("PushX", 0.0);
        state.pushZ = tag.getDouble("PushZ", 0.0);
        state.fuel = tag.getShort("Fuel", 0);
    }
    if (cart.type().path() == "tnt_minecart") {
        state.explosionPower = std::clamp(tag.getFloat("explosion_power", kTntCartPower), 0.0F, 128.0F);
        state.explosionSpeedFactor =
            std::clamp(tag.getFloat("explosion_speed_factor", kTntCartSpeedFactor), 0.0F, 128.0F);
    }
}

double speedOf(const ServerEntity& cart) { return std::abs(cart.velocityX) + std::abs(cart.velocityZ); }

} // namespace basalt::server::runtime::entity::minecarts
