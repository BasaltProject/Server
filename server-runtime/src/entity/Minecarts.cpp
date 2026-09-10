#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/EntityPhysics.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <numbers>
#include <set>
#include <string>
#include <string_view>

namespace basalt::server::runtime::entity::minecarts {
namespace {

/// parity: the 0.75 a ridden cart's own movement is scaled by.
constexpr double kRiddenScale = 0.75;

/// parity: the nudge a rider leaning on a key gives a cart that is barely moving.
constexpr double kRiderPush = 0.001;
constexpr double kRiderPushLimit = 0.01;

/// parity: `AbstractMinecart.push`, the shove a cart takes from whoever walks into it.
constexpr double kPushStrength = 0.1;
constexpr double kPushHalf = 0.5;
constexpr double kPushEpsilon = 1.0E-4;

/// parity: the `slideSpeed` a slope is worth.
constexpr double kSlopePush = 0.0078125;

/// parity: what a powered rail adds, and the nudge it gives a cart standing still.
constexpr double kBoost = 0.06;
constexpr double kNudge = 0.02;

/// parity: `getPos`'s own sixteenth of a block, and the twentieth of the drop it turns into speed.
constexpr double kRailLift = 0.0625;
constexpr double kSlopeSpeed = 0.05;

/// parity: `comeOffTrack`, which halves the speed on the ground and takes 0.95 of it in the air.
constexpr double kGroundDrag = 0.5;
constexpr double kAirDrag = 0.95;

/// parity: under this, an unpowered powered-rail stops the cart dead.
constexpr double kHaltSpeed = 0.03;

/// The two ways off a rail shape. parity: `AbstractMinecart.EXITS`.
struct Exits {
    std::array<std::int32_t, 3> a;
    std::array<std::int32_t, 3> b;
};

const std::map<std::string, Exits, std::less<>>& exits() {
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
    static const std::set<std::string, std::less<>> rails = {"rail", "powered_rail", "detector_rail", "activator_rail"};
    return rails.contains(path);
}

[[nodiscard]] bool isRail(const world::WorldState& world, const std::int32_t x, const std::int32_t y,
                          const std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    return block != nullptr && isRailPath(block->path());
}

[[nodiscard]] bool isSolid(const world::WorldState& world, const std::int32_t x, const std::int32_t y,
                           const std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() && registry::blockShapes::isFullCube(*stateId);
}

/// parity: a powered rail adds 0.06 along the way it is going, or nudges a still cart off a wall.
void boost(const world::WorldState& world, ServerEntity& cart, const std::array<std::int32_t, 3>& rail,
           std::string_view shape) {
    const double moving = std::sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ);
    if (moving > 0.01) {
        cart.velocityX += cart.velocityX / moving * kBoost;
        cart.velocityZ += cart.velocityZ / moving * kBoost;
        return;
    }
    if (shape == "east_west") {
        if (isSolid(world, rail[0] - 1, rail[1], rail[2])) {
            cart.velocityX = kNudge;
        } else if (isSolid(world, rail[0] + 1, rail[1], rail[2])) {
            cart.velocityX = -kNudge;
        }
    } else if (shape == "north_south") {
        if (isSolid(world, rail[0], rail[1], rail[2] - 1)) {
            cart.velocityZ = kNudge;
        } else if (isSolid(world, rail[0], rail[1], rail[2] + 1)) {
            cart.velocityZ = -kNudge;
        }
    }
}

} // namespace

std::optional<std::array<std::int32_t, 3>> railUnder(const world::WorldState& world, const ServerEntity& cart) {
    const std::int32_t x = wire::common::mth::floor(cart.x);
    const std::int32_t y = wire::common::mth::floor(cart.y);
    const std::int32_t z = wire::common::mth::floor(cart.z);
    if (isRail(world, x, y, z)) {
        return std::array<std::int32_t, 3>{x, y, z};
    }
    if (isRail(world, x, y - 1, z)) {
        return std::array<std::int32_t, 3>{x, y - 1, z};
    }
    return std::nullopt;
}

std::optional<std::array<double, 3>> railPosition(const world::WorldState& world, const double x, const double y,
                                                  const double z) {
    const std::int32_t blockX = wire::common::mth::floor(x);
    std::int32_t blockY = wire::common::mth::floor(y);
    const std::int32_t blockZ = wire::common::mth::floor(z);
    if (isRail(world, blockX, blockY - 1, blockZ)) {
        --blockY;
    }
    const std::optional<std::int32_t> stateId = world.blockState(blockX, blockY, blockZ);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr || !isRailPath(block->path())) {
        return std::nullopt;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto shape = properties.find("shape");
    if (shape == properties.end()) {
        return std::nullopt;
    }
    const auto found = exits().find(shape->second);
    if (found == exits().end()) {
        return std::nullopt;
    }
    const Exits& exit = found->second;
    const double fromX = blockX + 0.5 + exit.a[0] * 0.5;
    const double fromY = blockY + kRailLift + exit.a[1] * 0.5;
    const double fromZ = blockZ + 0.5 + exit.a[2] * 0.5;
    const double spanX = blockX + 0.5 + exit.b[0] * 0.5 - fromX;
    const double spanY = (blockY + kRailLift + exit.b[1] * 0.5 - fromY) * 2.0;
    const double spanZ = blockZ + 0.5 + exit.b[2] * 0.5 - fromZ;
    double progress = 0.0;
    if (spanX == 0.0) {
        progress = z - blockZ;
    } else if (spanZ == 0.0) {
        progress = x - blockX;
    } else {
        progress = ((x - fromX) * spanX + (z - fromZ) * spanZ) * 2.0;
    }
    double height = fromY + spanY * progress;
    if (spanY < 0.0) {
        height += 1.0;
    } else if (spanY > 0.0) {
        height += 0.5;
    }
    return std::array<double, 3>{fromX + spanX * progress, height, fromZ + spanZ * progress};
}

bool moveAlongTrack(world::WorldState& world, ServerEntity& cart, const double intentX, const double intentZ,
                    const bool ridden) {
    const std::optional<std::array<std::int32_t, 3>> rail = railUnder(world, cart);
    if (!rail.has_value()) {
        return false;
    }
    const std::optional<std::int32_t> stateId = world.blockState((*rail)[0], (*rail)[1], (*rail)[2]);
    if (!stateId.has_value()) {
        return false;
    }
    // parity: the height the cart is at *on the rail* before it moves, which is what tells a slope
    // from level ground once it has moved.
    const std::optional<std::array<double, 3>> oldRail = railPosition(world, cart.x, cart.y, cart.z);
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return false;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto shapeProperty = properties.find("shape");
    if (shapeProperty == properties.end()) {
        return false;
    }
    const std::string& shape = shapeProperty->second;
    const auto found = exits().find(shape);
    if (found == exits().end()) {
        return false;
    }
    const Exits& exit = found->second;

    bool powered = false;
    bool halting = false;
    if (block->path() == "powered_rail") {
        const auto poweredProperty = properties.find("powered");
        powered = poweredProperty != properties.end() && poweredProperty->second == "true";
        halting = !powered;
    }

    // parity: a slope pushes the cart downhill before anything else happens.
    if (shape == "ascending_east") {
        cart.velocityX -= kSlopePush;
    } else if (shape == "ascending_west") {
        cart.velocityX += kSlopePush;
    } else if (shape == "ascending_north") {
        cart.velocityZ += kSlopePush;
    } else if (shape == "ascending_south") {
        cart.velocityZ -= kSlopePush;
    }
    const std::int32_t railY = shape.starts_with("ascending") ? (*rail)[1] + 1 : (*rail)[1];

    // parity: the speed is projected onto the rail, keeping its size and taking its direction.
    double alongX = exit.b[0] - exit.a[0];
    double alongZ = exit.b[2] - exit.a[2];
    const double length = std::sqrt(alongX * alongX + alongZ * alongZ);
    if (cart.velocityX * alongX + cart.velocityZ * alongZ < 0.0) {
        alongX = -alongX;
        alongZ = -alongZ;
    }
    const double speed = std::min(2.0, std::sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ));
    cart.velocityX = speed * alongX / length;
    cart.velocityZ = speed * alongZ / length;

    // parity: a rider leaning on a key gets a stopped cart going again, which is also what stops an
    // unpowered powered rail from holding it there.
    if ((intentX != 0.0 || intentZ != 0.0) &&
        cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ < kRiderPushLimit) {
        const double intentLength = std::sqrt(intentX * intentX + intentZ * intentZ);
        if (intentLength > 0.0) {
            cart.velocityX += intentX / intentLength * kRiderPush;
            cart.velocityZ += intentZ / intentLength * kRiderPush;
            halting = false;
        }
    }
    // parity: an unpowered powered rail is a brake, and stops the cart under 0.03.
    if (halting) {
        const double moving = std::sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ);
        if (moving < kHaltSpeed) {
            cart.velocityX = 0.0;
            cart.velocityZ = 0.0;
        } else {
            cart.velocityX *= 0.5;
            cart.velocityZ *= 0.5;
        }
    }

    // parity: the cart is pinned to the line between the two exits rather than moving freely.
    const double fromX = (*rail)[0] + 0.5 + exit.a[0] * 0.5;
    const double fromZ = (*rail)[2] + 0.5 + exit.a[2] * 0.5;
    const double spanX = (*rail)[0] + 0.5 + exit.b[0] * 0.5 - fromX;
    const double spanZ = (*rail)[2] + 0.5 + exit.b[2] * 0.5 - fromZ;
    double progress = 0.0;
    if (spanX == 0.0) {
        progress = cart.z - (*rail)[2];
    } else if (spanZ == 0.0) {
        progress = cart.x - (*rail)[0];
    } else {
        progress = ((cart.x - fromX) * spanX + (cart.z - fromZ) * spanZ) * 2.0;
    }
    cart.x = fromX + spanX * progress;
    cart.y = railY;
    cart.z = fromZ + spanZ * progress;

    const double scale = ridden ? kRiddenScale : 1.0;
    cart.x += std::clamp(scale * cart.velocityX, -kMaxSpeed, kMaxSpeed);
    cart.z += std::clamp(scale * cart.velocityZ, -kMaxSpeed, kMaxSpeed);

    // parity: leaving the rail's own block down a slope drops the cart onto the next rail.
    const std::int32_t steppedX = wire::common::mth::floor(cart.x) - (*rail)[0];
    const std::int32_t steppedZ = wire::common::mth::floor(cart.z) - (*rail)[2];
    if (exit.a[1] != 0 && steppedX == exit.a[0] && steppedZ == exit.a[2]) {
        cart.y += exit.a[1];
    } else if (exit.b[1] != 0 && steppedX == exit.b[0] && steppedZ == exit.b[2]) {
        cart.y += exit.b[1];
    }

    // parity: `applyNaturalSlowdown`, the 0.96 a riderless cart keeps, and the y it drops.
    const double slowdown = ridden ? kRiddenSlowdown : kSlowdown;
    cart.velocityX *= slowdown;
    cart.velocityZ *= slowdown;
    cart.velocityY = 0.0;

    // parity: the twentieth of the height it lost, added to its speed. This is what makes a cart run
    // away downhill and crawl uphill instead of rolling at one speed everywhere.
    const std::optional<std::array<double, 3>> newRail = railPosition(world, cart.x, cart.y, cart.z);
    if (oldRail.has_value() && newRail.has_value()) {
        const double gained = ((*oldRail)[1] - (*newRail)[1]) * kSlopeSpeed;
        const double moving = std::sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ);
        if (moving > 0.0) {
            cart.velocityX *= (moving + gained) / moving;
            cart.velocityZ *= (moving + gained) / moving;
        }
        cart.y = (*newRail)[1];
    }

    // parity: a cart that crossed into the next block points its speed the way it stepped.
    const std::int32_t steppedIntoX = wire::common::mth::floor(cart.x);
    const std::int32_t steppedIntoZ = wire::common::mth::floor(cart.z);
    if (steppedIntoX != (*rail)[0] || steppedIntoZ != (*rail)[2]) {
        const double moving = std::sqrt(cart.velocityX * cart.velocityX + cart.velocityZ * cart.velocityZ);
        cart.velocityX = moving * (steppedIntoX - (*rail)[0]);
        cart.velocityZ = moving * (steppedIntoZ - (*rail)[2]);
    }

    if (powered) {
        boost(world, cart, *rail, shape);
    }
    cart.onGround = true;
    return true;
}

void activateOnRail(world::WorldState& world, ServerEntity& cart, const bool tntExplodes) {
    const std::optional<std::array<std::int32_t, 3>> rail = railUnder(world, cart);
    if (!rail.has_value()) {
        return;
    }
    const std::optional<std::int32_t> stateId = world.blockState((*rail)[0], (*rail)[1], (*rail)[2]);
    const wire::common::Identifier* block =
        stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
    if (block == nullptr || block->path() != "activator_rail") {
        return;
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto powered = properties.find("powered");
    const bool on = powered != properties.end() && powered->second == "true";
    const std::string_view path = cart.type().path();
    if (path == "hopper_minecart") {
        // parity: `MinecartHopper.activateMinecart`, `enabled = !powered`, written down as `Enabled`.
        if (!cart.sourceNbt.has_value()) {
            cart.sourceNbt = wire::nbt::NbtCompound{};
        }
        cart.sourceNbt->putBoolean("Enabled", !on);
        return;
    }
    // parity: `MinecartTNT.activateMinecart`, which lights a fuse that is not lit already, and
    // `primeFuse`, which does nothing at all with the `tnt_explodes` rule off.
    if (path == "tnt_minecart" && on && cart.fuse < 0 && tntExplodes) {
        cart.fuse = kTntCartFuse;
    }
}

void comeOffTrack(world::WorldState& world, ServerEntity& cart, const double gravity) {
    cart.velocityX = std::clamp(cart.velocityX, -kMaxSpeed, kMaxSpeed);
    cart.velocityZ = std::clamp(cart.velocityZ, -kMaxSpeed, kMaxSpeed);
    if (cart.onGround) {
        cart.velocityX *= kGroundDrag;
        cart.velocityY *= kGroundDrag;
        cart.velocityZ *= kGroundDrag;
    }
    entityPhysics::move(world, cart, gravity, 1.0);
    if (!cart.onGround) {
        cart.velocityX *= kAirDrag;
        cart.velocityY *= kAirDrag;
        cart.velocityZ *= kAirDrag;
    }
}

std::array<double, 2> intentOf(const bool forward, const bool back, const bool left, const bool right,
                               const float yaw) {
    const double sideways = (left ? 1.0 : 0.0) - (right ? 1.0 : 0.0);
    const double ahead = (forward ? 1.0 : 0.0) - (back ? 1.0 : 0.0);
    const double length = std::sqrt(sideways * sideways + ahead * ahead);
    if (length == 0.0) {
        return {0.0, 0.0};
    }
    const double x = sideways / length;
    const double z = ahead / length;
    const double radians = static_cast<double>(yaw) * std::numbers::pi / 180.0;
    const double sin = std::sin(radians);
    const double cos = std::cos(radians);
    return {x * cos - z * sin, z * cos + x * sin};
}

void pushedBy(ServerEntity& cart, const double fromX, const double fromZ) {
    double dx = fromX - cart.x;
    double dz = fromZ - cart.z;
    const double squared = dx * dx + dz * dz;
    if (squared < kPushEpsilon) {
        return;
    }
    const double distance = std::sqrt(squared);
    dx /= distance;
    dz /= distance;
    const double strength = std::min(1.0 / distance, 1.0) * kPushStrength * kPushHalf;
    cart.velocityX -= dx * strength;
    cart.velocityZ -= dz * strength;
}

double speedOf(const ServerEntity& cart) { return std::abs(cart.velocityX) + std::abs(cart.velocityZ); }

} // namespace basalt::server::runtime::entity::minecarts
