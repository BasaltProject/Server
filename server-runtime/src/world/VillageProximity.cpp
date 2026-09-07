#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/VillageProximity.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::world::villageProximity {
namespace {

constexpr std::int32_t kSectionBits = 4;

[[nodiscard]] bool isLiveVillager(const entity::ServerEntity& entity) {
    return !entity.removed && entity.type().path() == "villager";
}

[[nodiscard]] std::int32_t sectionOf(double coordinate) {
    return static_cast<std::int32_t>(std::floor(coordinate)) >> kSectionBits;
}

} // namespace

bool isCloseToVillage(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                      std::int32_t sectionDistance) {
    const std::int32_t sectionX = x >> kSectionBits;
    const std::int32_t sectionY = y >> kSectionBits;
    const std::int32_t sectionZ = z >> kSectionBits;
    for (const entity::ServerEntity* entity : world.entities()) {
        if (entity == nullptr || !isLiveVillager(*entity)) {
            continue;
        }
        const std::int32_t distance =
            std::max({std::abs(sectionOf(entity->x) - sectionX), std::abs(sectionOf(entity->y) - sectionY),
                      std::abs(sectionOf(entity->z) - sectionZ)});
        if (distance <= sectionDistance) {
            return true;
        }
    }
    return false;
}

std::int32_t occupiedHomesNear(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                               std::int32_t radius) {
    const auto radiusSquared = static_cast<std::int64_t>(radius) * radius;
    std::int32_t found = 0;
    for (const entity::ServerEntity* entity : world.entities()) {
        if (entity == nullptr || !isLiveVillager(*entity)) {
            continue;
        }
        const auto dx = static_cast<std::int64_t>(std::floor(entity->x)) - x;
        const auto dy = static_cast<std::int64_t>(std::floor(entity->y)) - y;
        const auto dz = static_cast<std::int64_t>(std::floor(entity->z)) - z;
        if (dx * dx + dy * dy + dz * dz <= radiusSquared) {
            found++;
        }
    }
    return found;
}

} // namespace basalt::server::runtime::world::villageProximity
