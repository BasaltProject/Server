#include <basalt/server/runtime/entity/Projectiles.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string_view>

namespace basalt::server::runtime::entity::projectiles {
namespace {

/// parity: `getMovementToShoot`, the spread one degree of uncertainty is worth.
constexpr double kSpreadPerUncertainty = 0.0172275;

constexpr double kDegrees = 180.0 / std::numbers::pi;

/// parity: `AbstractHurtingProjectile.INITAL_ACCELERATION_POWER`, the misspelling included.
constexpr double kAcceleration = 0.1;

[[nodiscard]] Kind make(std::string_view path, double gravity, double drag, bool sticks, double acceleration = 0.0) {
    return Kind{.type = wire::common::Identifier::vanilla(std::string{path}),
                .gravity = gravity,
                .drag = drag,
                .sticks = sticks,
                .acceleration = acceleration};
}

/// parity: `AbstractArrow.onHitBlock` scales by a `0.05F`, and the float widened to a double is not
/// the same number as `0.05`. The arrow ends up a fraction off otherwise.
constexpr double kStuckBackoff = static_cast<double>(0.05F);

/// parity: `Vec3.normalize`, which answers zero rather than dividing when the vector is this short.
constexpr double kNormalizeFloor = 1.0E-4;

/// parity: `Math.signum`, which is what vanilla backs the arrow off along and not the direction.
[[nodiscard]] double signum(double value) { return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0); }

/// parity: the two `atan2` calls that turn the movement into a yaw and a pitch.
void aimAlongMovement(ServerEntity& entity) {
    const double horizontal = std::sqrt((entity.velocityX * entity.velocityX) + (entity.velocityZ * entity.velocityZ));
    entity.yaw = static_cast<float>(std::atan2(entity.velocityX, entity.velocityZ) * kDegrees);
    entity.pitch = static_cast<float>(std::atan2(entity.velocityY, horizontal) * kDegrees);
}

} // namespace

const Kind& arrow() {
    static const Kind instance = make("arrow", 0.05, 0.99, true);
    return instance;
}

const Kind& fireball() {
    static const Kind instance = make("fireball", 0.0, 0.95, false, kAcceleration);
    return instance;
}

const Kind& snowball() {
    static const Kind instance = make("snowball", 0.03, 0.99, false);
    return instance;
}

std::span<const Kind> all() {
    static const std::array<Kind, 10> kinds{
        arrow(), make("spectral_arrow", 0.05, 0.99, true), snowball(), make("egg", 0.03, 0.99, false),
        make("ender_pearl", 0.03, 0.99, false), make("experience_bottle", 0.07, 0.99, false),
        make("potion", 0.05, 0.99, false), fireball(), make("small_fireball", 0.0, 0.95, false, kAcceleration),
        // parity: `AbstractWindCharge` neither slows down nor speeds up: inertia one, acceleration
        // zeroed in its constructor.
        make("wind_charge", 0.0, 1.0, false)};
    return kinds;
}

std::optional<DispenseShot> dispensed(const wire::common::Identifier& item) {
    /// parity: the items `DispenseItemBehavior.bootStrap` registers a projectile behaviour for, each
    /// with the `createDispenseConfig` its item overrides. The ones this build has no entity for
    /// (fireworks and lingering potions) are not here and fall back to being thrown.
    struct Entry {
        std::string_view item;
        std::string_view kind;
        double reach = kDispenserReach;
        double lift = kDispenserLift;
        float power = kDispenserPower;
        float uncertainty = kDispenserUncertainty;
        bool carries = true;
    };

    /// parity: `ThrowablePotionItem.createDispenseConfig`, a quarter harder and half as wide.
    constexpr float kPotionPower = kDispenserPower * 1.25F;
    constexpr float kPotionUncertainty = kDispenserUncertainty * 0.5F;
    /// parity: `FireChargeItem.createDispenseConfig` and `WindChargeItem`'s, the same numbers: a
    /// whole block out with no lift at all.
    constexpr double kChargeReach = 1.0;
    constexpr float kChargePower = 1.0F;
    constexpr float kChargeUncertainty = 6.6666665F;
    static const std::array<Entry, 11> table{
        Entry{.item = "arrow", .kind = "arrow", .carries = false},
        Entry{.item = "tipped_arrow", .kind = "arrow", .carries = false},
        Entry{.item = "spectral_arrow", .kind = "spectral_arrow", .carries = false},
        Entry{.item = "egg", .kind = "egg"},
        Entry{.item = "blue_egg", .kind = "egg"},
        Entry{.item = "brown_egg", .kind = "egg"},
        Entry{.item = "snowball", .kind = "snowball"},
        Entry{.item = "experience_bottle", .kind = "experience_bottle"},
        Entry{.item = "splash_potion", .kind = "potion", .power = kPotionPower, .uncertainty = kPotionUncertainty},
        Entry{.item = "wind_charge",
              .kind = "wind_charge",
              .reach = kChargeReach,
              .lift = 0.0,
              .power = kChargePower,
              .uncertainty = kChargeUncertainty,
              .carries = false},
        Entry{.item = "fire_charge",
              .kind = "small_fireball",
              .reach = kChargeReach,
              .lift = 0.0,
              .power = kChargePower,
              .uncertainty = kChargeUncertainty}};
    const auto found = std::ranges::find_if(table, [&](const Entry& one) { return one.item == item.path(); });
    if (found == table.end()) {
        return std::nullopt;
    }
    const auto kind = std::ranges::find_if(all(), [&](const Kind& one) { return one.type.path() == found->kind; });
    if (kind == all().end()) {
        return std::nullopt;
    }
    return DispenseShot{.kind = &*kind,
                        .reach = found->reach,
                        .lift = found->lift,
                        .power = found->power,
                        .uncertainty = found->uncertainty,
                        .carries = found->carries};
}

const Kind* kindOf(const wire::common::Identifier& type) {
    const auto found = std::ranges::find_if(all(), [&](const Kind& one) { return one.type == type; });
    return found == all().end() ? nullptr : &*found;
}

ServerEntity& spawn(world::WorldState& world, const Kind& kind, std::int32_t entityId, std::int32_t protocolType,
                    double x, double y, double z, double dirX, double dirY, double dirZ, float power, float uncertainty,
                    random::RandomSource& random) {
    auto entity =
        std::make_unique<ServerEntity>(entityId, wire::common::Uuid::random(), kind.type, protocolType, x, y, z);
    shoot(*entity, dirX, dirY, dirZ, power, uncertainty, random);
    return world.spawnEntity(std::move(entity));
}

void shoot(ServerEntity& entity, double dirX, double dirY, double dirZ, float power, float uncertainty,
           random::RandomSource& random) {
    const double length = std::sqrt((dirX * dirX) + (dirY * dirY) + (dirZ * dirZ));
    if (length == 0.0) {
        return;
    }
    const double spread = kSpreadPerUncertainty * static_cast<double>(uncertainty);
    const auto scale = static_cast<double>(power);
    entity.velocityX = ((dirX / length) + random.triangle(0.0, spread)) * scale;
    entity.velocityY = ((dirY / length) + random.triangle(0.0, spread)) * scale;
    entity.velocityZ = ((dirZ / length) + random.triangle(0.0, spread)) * scale;
    aimAlongMovement(entity);
}

std::optional<BlockHit> tick(world::WorldState& world, ServerEntity& entity, const Kind& kind) {
    ++entity.age;
    if (entity.onGround) {
        return std::nullopt;
    }

    // parity: `AbstractHurtingProjectile.applyInertia` adds the acceleration back along the way it
    // is going before the drag, so a fireball settles at a speed rather than stopping. The other two
    // families have no acceleration and the same line is their plain scale by the inertia.
    const double speed = std::sqrt((entity.velocityX * entity.velocityX) + (entity.velocityY * entity.velocityY) +
                                   (entity.velocityZ * entity.velocityZ));
    if (kind.acceleration != 0.0 && speed >= kNormalizeFloor) {
        entity.velocityX += (entity.velocityX / speed) * kind.acceleration;
        entity.velocityY += (entity.velocityY / speed) * kind.acceleration;
        entity.velocityZ += (entity.velocityZ / speed) * kind.acceleration;
    }
    entity.velocityX *= kind.drag;
    entity.velocityY *= kind.drag;
    entity.velocityZ *= kind.drag;

    const std::optional<BlockHit> hit = blockRay::cast(world, entity.x, entity.y, entity.z, entity.x + entity.velocityX,
                                                       entity.y + entity.velocityY, entity.z + entity.velocityZ);
    if (hit.has_value()) {
        entity.x = hit->x;
        entity.y = hit->y;
        entity.z = hit->z;
        if (kind.sticks) {
            // parity: `AbstractArrow.onHitBlock` backs the arrow off by a twentieth of a block along
            // each axis it was moving on, **signum and not the normalized direction**, so it ends up
            // in the square in front of what it hit rather than on its face. Measured in
            // `flechavibra`, where ours stuck one square further along than the jar's.
            entity.x -= signum(entity.velocityX) * kStuckBackoff;
            entity.y -= signum(entity.velocityY) * kStuckBackoff;
            entity.z -= signum(entity.velocityZ) * kStuckBackoff;
            // parity: `AbstractArrow.setInGround`, which zeroes the movement and leaves it stuck.
            entity.velocityX = 0.0;
            entity.velocityY = 0.0;
            entity.velocityZ = 0.0;
            entity.onGround = true;
        }
        return hit;
    }

    entity.x += entity.velocityX;
    entity.y += entity.velocityY;
    entity.z += entity.velocityZ;
    // parity: `Entity.applyGravity` returns at once when `isNoGravity`, so a projectile with the tag
    // flies dead straight. Measured in `botonflecha`, where a flecha that should hang at half a
    // block fell into the button and pressed it.
    if (!entity.noGravity) {
        entity.velocityY -= kind.gravity;
    }
    aimAlongMovement(entity);
    return std::nullopt;
}

std::optional<double> clipBox(double fromX, double fromY, double fromZ, double toX, double toY, double toZ, double minX,
                              double minY, double minZ, double maxX, double maxY, double maxZ) {
    double enter = 0.0;
    double exit = 1.0;
    const std::array<std::array<double, 4>, 3> axes{
        {{fromX, toX - fromX, minX, maxX}, {fromY, toY - fromY, minY, maxY}, {fromZ, toZ - fromZ, minZ, maxZ}}};
    for (const std::array<double, 4>& axis : axes) {
        const double start = axis[0];
        const double delta = axis[1];
        const double low = axis[2];
        const double high = axis[3];
        if (delta == 0.0) {
            if (start < low || start > high) {
                return std::nullopt;
            }
            continue;
        }
        const double first = (low - start) / delta;
        const double second = (high - start) / delta;
        enter = std::max(enter, std::min(first, second));
        exit = std::min(exit, std::max(first, second));
        if (enter > exit) {
            return std::nullopt;
        }
    }
    return enter;
}

} // namespace basalt::server::runtime::entity::projectiles
