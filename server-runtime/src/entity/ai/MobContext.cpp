#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime::entity::ai {

std::int32_t MobContext::lightLevel() const {
    if (lightCache < 0) {
        const std::int32_t block =
            world::worldLight::blockLight(world, lightPosition[0], lightPosition[1], lightPosition[2]);
        const std::int32_t sky =
            world::worldLight::skyLight(world, lightPosition[0], lightPosition[1], lightPosition[2]);
        // parity: `Entity.getLightLevelDependentMagicValue`.
        lightCache = std::max(block, night ? 0 : sky);
    }
    return lightCache;
}

const PlayerView* MobContext::nearestPlayer(double range) const {
    const PlayerView* best = nullptr;
    double bestDistance = range * range;
    for (const PlayerView& player : players) {
        if (!player.targetable) {
            continue;
        }
        const double distance = distanceSquaredTo(player.x, player.y, player.z);
        if (distance > bestDistance) {
            continue;
        }
        bestDistance = distance;
        best = &player;
    }
    return best;
}

double MobContext::distanceSquaredTo(double x, double y, double z) const {
    const double dx = mob.x - x;
    const double dy = mob.y - y;
    const double dz = mob.z - z;
    return dx * dx + dy * dy + dz * dz;
}

const PlayerView* MobContext::playerById(std::int32_t entityId) const {
    for (const PlayerView& player : players) {
        if (player.entityId == entityId) {
            return &player;
        }
    }
    return nullptr;
}

ServerEntity* MobContext::entityById(std::int32_t entityId) const {
    for (ServerEntity* entity : entities) {
        if (entity != nullptr && entity->entityId() == entityId) {
            return entity;
        }
    }
    return nullptr;
}

} // namespace basalt::server::runtime::entity::ai
