#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <string_view>

namespace basalt::server::runtime::entity::projectiles {
namespace {

/// parity: `getMovementToShoot`, the spread one degree of uncertainty is worth.
constexpr double kSpreadPerUncertainty = 0.0172275;

constexpr double kDegrees = 180.0 / std::numbers::pi;

/// parity: `AbstractHurtingProjectile.INITAL_ACCELERATION_POWER`.
constexpr double kAcceleration = 0.1;

[[nodiscard]] Kind make(std::string_view path, double gravity, double drag, bool sticks, double acceleration = 0.0) {
    return Kind{.type = wire::common::Identifier::vanilla(std::string{path}),
                .gravity = gravity,
                .drag = drag,
                .sticks = sticks,
                .acceleration = acceleration,
                // parity: which of the three `tick` bodies it has.
                .flight = sticks ? Flight::Arrow : (acceleration != 0.0 ? Flight::Hurting : Flight::Thrown)};
}

/// parity: `AbstractArrow.onHitBlock` scales by the float `0.05F`.
constexpr double kStuckBackoff = static_cast<double>(0.05F);

/// parity: `Vec3.normalize`.
constexpr double kNormalizeFloor = 1.0E-4;

/// parity: `Math.signum`.
[[nodiscard]] double signum(double value) { return value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0); }

/// parity: the two `atan2` calls that turn the movement into a yaw and a pitch.
void aimAlongMovement(ServerEntity& entity) {
    const double horizontal = std::sqrt((entity.velocityX * entity.velocityX) + (entity.velocityZ * entity.velocityZ));
    entity.yaw = static_cast<float>(std::atan2(entity.velocityX, entity.velocityZ) * kDegrees);
    entity.pitch = static_cast<float>(std::atan2(entity.velocityY, horizontal) * kDegrees);
}

/// parity: `AbstractArrow.tick`'s `aabb.move(blockPos).contains(position)`.
[[nodiscard]] bool insideCollision(world::WorldState& world, double x, double y, double z) {
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockY = static_cast<std::int32_t>(std::floor(y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    const std::optional<std::int32_t> stateId = world.blockState(blockX, blockY, blockZ);
    if (!stateId.has_value() || registry::blockShapes::isEmptyShape(*stateId)) {
        return false;
    }
    constexpr double kUnits = 16.0;
    const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(*stateId);
    for (std::size_t box = 0; box + 5 < boxes.size(); box += 6) {
        const bool inside = x >= blockX + (boxes[box] / kUnits) && x < blockX + (boxes[box + 3] / kUnits) &&
                            y >= blockY + (boxes[box + 1] / kUnits) && y < blockY + (boxes[box + 4] / kUnits) &&
                            z >= blockZ + (boxes[box + 2] / kUnits) && z < blockZ + (boxes[box + 5] / kUnits);
        if (inside) {
            return true;
        }
    }
    return false;
}

/// parity: `AbstractArrow.shouldFall`.
[[nodiscard]] bool nothingHolds(world::WorldState& world, double x, double y, double z) {
    constexpr double kGrip = 0.06;
    constexpr double kUnits = 16.0;
    const auto low = [](double value) { return static_cast<std::int32_t>(std::floor(value - kGrip)); };
    const auto high = [](double value) { return static_cast<std::int32_t>(std::floor(value + kGrip)); };
    for (std::int32_t blockX = low(x); blockX <= high(x); ++blockX) {
        for (std::int32_t blockY = low(y); blockY <= high(y); ++blockY) {
            for (std::int32_t blockZ = low(z); blockZ <= high(z); ++blockZ) {
                const std::optional<std::int32_t> stateId = world.blockState(blockX, blockY, blockZ);
                if (!stateId.has_value() || registry::blockShapes::isEmptyShape(*stateId)) {
                    continue;
                }
                const std::span<const std::int32_t> boxes = registry::blockShapes::collisionBoxes(*stateId);
                for (std::size_t box = 0; box + 5 < boxes.size(); box += 6) {
                    const bool touches = x + kGrip > blockX + (boxes[box] / kUnits) &&
                                         x - kGrip < blockX + (boxes[box + 3] / kUnits) &&
                                         y + kGrip > blockY + (boxes[box + 1] / kUnits) &&
                                         y - kGrip < blockY + (boxes[box + 4] / kUnits) &&
                                         z + kGrip > blockZ + (boxes[box + 2] / kUnits) &&
                                         z - kGrip < blockZ + (boxes[box + 5] / kUnits);
                    if (touches) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
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
    static const std::array<Kind, 16> kinds{
        arrow(), make("spectral_arrow", 0.05, 0.99, true),
        // parity: `ThrownTrident` is an arrow whose water inertia is its air drag.
        make("trident", 0.05, 0.99, true), snowball(), make("egg", 0.03, 0.99, false),
        make("ender_pearl", 0.03, 0.99, false), make("experience_bottle", 0.07, 0.99, false),
        make("splash_potion", 0.05, 0.99, false), make("lingering_potion", 0.05, 0.99, false), fireball(),
        make("small_fireball", 0.0, 0.95, false, kAcceleration),
        // parity: `AbstractWindCharge` has inertia one and acceleration zeroed in its constructor.
        make("wind_charge", 0.0, 1.0, false), make("breeze_wind_charge", 0.0, 1.0, false),
        make("dragon_fireball", 0.0, 0.95, false, kAcceleration), make("wither_skull", 0.0, 0.95, false, kAcceleration),
        // parity: `LlamaSpit.getDefaultGravity` and `getAirDrag` (a float widened).
        make("llama_spit", 0.06, static_cast<double>(0.99F), false)};
    return kinds;
}

std::optional<DispenseShot> dispensed(const wire::common::Identifier& item) {
    /// parity: `DispenseItemBehavior.bootStrap`.
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
    /// parity: `FireChargeItem.createDispenseConfig` and `WindChargeItem`'s.
    constexpr double kChargeReach = 1.0;
    constexpr float kChargePower = 1.0F;
    constexpr float kChargeUncertainty = 6.6666665F;
    static const std::array<Entry, 13> table{
        // parity: `TridentItem.asProjectile`, the default dispense config.
        Entry{.item = "trident", .kind = "trident"},
        Entry{.item = "arrow", .kind = "arrow", .carries = false},
        Entry{.item = "tipped_arrow", .kind = "arrow", .carries = false},
        Entry{.item = "spectral_arrow", .kind = "spectral_arrow", .carries = false},
        Entry{.item = "egg", .kind = "egg"},
        Entry{.item = "blue_egg", .kind = "egg"},
        Entry{.item = "brown_egg", .kind = "egg"},
        Entry{.item = "snowball", .kind = "snowball"},
        Entry{.item = "experience_bottle", .kind = "experience_bottle"},
        Entry{
            .item = "splash_potion", .kind = "splash_potion", .power = kPotionPower, .uncertainty = kPotionUncertainty},
        Entry{.item = "lingering_potion",
              .kind = "lingering_potion",
              .power = kPotionPower,
              .uncertainty = kPotionUncertainty},
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
    // parity: `AbstractArrow.tick` without physics.
    if (kind.sticks && entity.noPhysics) {
        entity.onGround = false;
        entity.inGroundTime = 0;
        entity.x += entity.velocityX;
        entity.y += entity.velocityY;
        entity.z += entity.velocityZ;
        entity.velocityX *= kind.drag;
        entity.velocityY *= kind.drag;
        entity.velocityZ *= kind.drag;
        const double horizontal =
            std::sqrt((entity.velocityX * entity.velocityX) + (entity.velocityZ * entity.velocityZ));
        entity.yaw = static_cast<float>(std::atan2(-entity.velocityX, -entity.velocityZ) * kDegrees);
        entity.pitch = static_cast<float>(std::atan2(entity.velocityY, horizontal) * kDegrees);
        return std::nullopt;
    }
    if (entity.onGround) {
        // parity: `AbstractArrow.tick`.
        if (!kind.sticks) {
            return std::nullopt;
        }
        if (!nothingHolds(world, entity.x, entity.y, entity.z)) {
            // parity: `tickDespawn`, a minute in the ground and gone, save a trident that comes home.
            const bool keeps = entity.type().path() == "trident" && entity.tridentLoyalty > 0 &&
                               entity.arrowPickup == ServerEntity::ArrowPickup::Allowed;
            const config::MobSettings& mobs = world.mobSettings();
            const std::int32_t limit =
                entity.type().path() == "trident" ? mobs.tridentDespawnTicks : mobs.arrowDespawnTicks;
            if (!keeps && ++entity.arrowLife >= limit) {
                entity.removed = true;
            }
            ++entity.inGroundTime;
            return std::nullopt;
        }
        // parity: the in-ground branch ends the turn, so the tick an arrow lets go on it does not move.
        entity.onGround = false;
        return std::nullopt;
    }
    // parity: `AbstractArrow.tick`.
    if (kind.sticks && insideCollision(world, entity.x, entity.y, entity.z)) {
        entity.velocityX = 0.0;
        entity.velocityY = 0.0;
        entity.velocityZ = 0.0;
        entity.onGround = true;
        return std::nullopt;
    }

    // parity: `WitherSkull.getInertia`, 0.73 for a blue one.
    const double drag = entity.dangerousSkull ? static_cast<double>(0.73F) : kind.drag;
    const auto slow = [&entity, drag] {
        entity.velocityX *= drag;
        entity.velocityY *= drag;
        entity.velocityZ *= drag;
    };
    // parity: `Entity.applyGravity` returns early when `isNoGravity`.
    const auto fall = [&entity, &kind] {
        if (!entity.noGravity) {
            entity.velocityY -= kind.gravity;
        }
    };
    // parity: `ThrowableProjectile.tick` applies gravity then inertia before the move.
    if (kind.flight == Flight::Thrown) {
        fall();
        slow();
    }
    // parity: `AbstractHurtingProjectile.applyInertia`.
    if (kind.flight == Flight::Hurting) {
        const double speed = std::sqrt((entity.velocityX * entity.velocityX) + (entity.velocityY * entity.velocityY) +
                                       (entity.velocityZ * entity.velocityZ));
        if (kind.acceleration != 0.0 && speed >= kNormalizeFloor) {
            entity.velocityX += (entity.velocityX / speed) * kind.acceleration;
            entity.velocityY += (entity.velocityY / speed) * kind.acceleration;
            entity.velocityZ += (entity.velocityZ / speed) * kind.acceleration;
        }
        slow();
    }

    entity.inGroundTime = 0;
    const std::optional<BlockHit> hit = blockRay::cast(world, entity.x, entity.y, entity.z, entity.x + entity.velocityX,
                                                       entity.y + entity.velocityY, entity.z + entity.velocityZ);
    if (hit.has_value()) {
        entity.x = hit->x;
        entity.y = hit->y;
        entity.z = hit->z;
        if (kind.sticks) {
            // parity: `AbstractArrow.onHitBlock`.
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
    // parity: an arrow moves first and slows and falls after, so its first tick goes the whole way the bow sent it.
    if (kind.flight == Flight::Arrow) {
        slow();
        fall();
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
