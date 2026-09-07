#include <basalt/server/runtime/entity/Projectiles.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string_view>
#include <unordered_map>

namespace basalt::server::runtime::entity::projectiles {
namespace {

/// parity: `getMovementToShoot`, the spread one degree of uncertainty is worth.
constexpr double kSpreadPerUncertainty = 0.0172275;

constexpr double kDegrees = 180.0 / std::numbers::pi;

[[nodiscard]] Kind make(std::string_view path, double gravity, double drag, bool sticks) {
    return Kind{.type = wire::common::Identifier::vanilla(std::string{path}),
                .gravity = gravity,
                .drag = drag,
                .sticks = sticks};
}

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
    static const Kind instance = make("fireball", 0.0, 0.95, false);
    return instance;
}

const Kind& snowball() {
    static const Kind instance = make("snowball", 0.03, 0.99, false);
    return instance;
}

std::span<const Kind> all() {
    static const std::array<Kind, 9> kinds{arrow(),
                                           make("spectral_arrow", 0.05, 0.99, true),
                                           snowball(),
                                           make("egg", 0.03, 0.99, false),
                                           make("ender_pearl", 0.03, 0.99, false),
                                           make("experience_bottle", 0.07, 0.99, false),
                                           make("potion", 0.05, 0.99, false),
                                           fireball(),
                                           make("small_fireball", 0.0, 0.95, false)};
    return kinds;
}

const Kind* dispensed(const wire::common::Identifier& item) {
    /// parity: the items `DispenseItemBehavior.bootStrap` registers a projectile behaviour for. The
    /// ones this build has no entity for (fireworks, wind charges, lingering potions) are not here
    /// and fall back to being thrown as an item.
    static const std::unordered_map<std::string_view, std::string_view> table{
        {"arrow", "arrow"},
        {"tipped_arrow", "arrow"},
        {"spectral_arrow", "spectral_arrow"},
        {"egg", "egg"},
        {"blue_egg", "egg"},
        {"brown_egg", "egg"},
        {"snowball", "snowball"},
        {"experience_bottle", "experience_bottle"},
        {"splash_potion", "potion"},
    };
    const auto found = table.find(item.path());
    if (found == table.end()) {
        return nullptr;
    }
    const auto kind = std::ranges::find_if(all(), [&](const Kind& one) { return one.type.path() == found->second; });
    return kind == all().end() ? nullptr : &*kind;
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
    entity.velocityY -= kind.gravity;
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
