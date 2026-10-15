#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/VillageGolemSpawner.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>

namespace basalt::server::runtime::world::villageGolemSpawner {
namespace {

namespace reg = server::registry;

using entity::Aabb;
using entity::ServerEntity;
using wire::common::Identifier;

constexpr std::int32_t kSpawnAttempts = 10;
constexpr std::int32_t kSpawnRangeXz = 8;
constexpr std::int32_t kSpawnRangeY = 6;

[[nodiscard]] const Identifier& ironGolem() {
    static const Identifier type = Identifier::vanilla("iron_golem");
    return type;
}

[[nodiscard]] const std::set<std::string, std::less<>>& legacyRejectedSupports() {
    static const std::set<std::string, std::less<>> all{"cobweb",      "cactus",      "glass_pane",   "conduit",
                                                        "ice",         "tnt",         "glowstone",    "beacon",
                                                        "sea_lantern", "frosted_ice", "tinted_glass", "glass"};
    return all;
}

/// parity: `SpawnUtil.Strategy.LEGACY_IRON_GOLEM`.
[[nodiscard]] bool legacySupport(std::int32_t supportState, std::int32_t aboveState) {
    const Identifier* block = reg::blockStateDefinitions::blockOf(supportState);
    if (block == nullptr) {
        return false;
    }
    const std::string& support = block->path();
    if (legacyRejectedSupports().contains(support) || support.ends_with("_stained_glass") ||
        support.ends_with("_stained_glass_pane") || reg::blockStateFamilies::isLeavesState(supportState)) {
        return false;
    }
    const bool clearAbove = reg::blocks::isAir(aboveState) || reg::blockFluids::hasFluid(aboveState);
    return clearAbove && (reg::blockShapes::isSolidState(supportState) || support == "powder_snow");
}

/// parity: `Mob.checkSpawnObstruction` rejects any fluid the completed golem's box touches.
[[nodiscard]] bool containsFluid(LoadedWorld& world, const Aabb& box) {
    const auto maxX = static_cast<std::int32_t>(std::floor(box.maxX - 1.0e-7));
    const auto maxY = static_cast<std::int32_t>(std::floor(box.maxY - 1.0e-7));
    const auto maxZ = static_cast<std::int32_t>(std::floor(box.maxZ - 1.0e-7));
    for (auto x = static_cast<std::int32_t>(std::floor(box.minX)); x <= maxX; x++) {
        for (auto y = static_cast<std::int32_t>(std::floor(box.minY)); y <= maxY; y++) {
            for (auto z = static_cast<std::int32_t>(std::floor(box.minZ)); z <= maxZ; z++) {
                const std::optional<std::int32_t> state = world.state().blockState(x, y, z);
                if (!state.has_value() || reg::blockFluids::hasFluid(*state)) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

std::optional<std::array<std::int32_t, 3>> findSpawnPosition(LoadedWorld& world, const ServerEntity& villager,
                                                             random::RandomSource& random) {
    const auto startX = static_cast<std::int32_t>(std::floor(villager.x));
    const auto startY = static_cast<std::int32_t>(std::floor(villager.y));
    const auto startZ = static_cast<std::int32_t>(std::floor(villager.z));
    for (std::int32_t attempt = 0; attempt < kSpawnAttempts; attempt++) {
        const std::int32_t x = startX + random.nextIntBetweenInclusive(-kSpawnRangeXz, kSpawnRangeXz);
        const std::int32_t z = startZ + random.nextIntBetweenInclusive(-kSpawnRangeXz, kSpawnRangeXz);
        if (world.border().distanceOutside(x + 0.5, z + 0.5) > 0.0) {
            continue;
        }
        std::int32_t aboveY = startY + kSpawnRangeY;
        std::optional<std::int32_t> above = world.state().blockState(x, aboveY, z);
        if (!above.has_value()) {
            continue;
        }
        for (std::int32_t step = 0; step < kSpawnRangeY * 2 + 1; step++) {
            const std::int32_t supportY = aboveY - 1;
            const std::optional<std::int32_t> support = world.state().blockState(x, supportY, z);
            if (!support.has_value()) {
                break;
            }
            if (legacySupport(*support, *above)) {
                const std::int32_t spawnY = supportY + 1;
                const Aabb box =
                    Aabb::of(x + 0.5, static_cast<double>(spawnY), z + 0.5, reg::entityTypeShapes::widthOf(ironGolem()),
                             reg::entityTypeShapes::heightOf(ironGolem()));
                const bool blocked = entity::entityCollision::collides(
                    [&world](std::int32_t bx, std::int32_t by, std::int32_t bz) {
                        return world.state().blockState(bx, by, bz);
                    },
                    box);
                if (!blocked && !containsFluid(world, box)) {
                    return std::array<std::int32_t, 3>{x, spawnY, z};
                }
                // parity: the first support found decides the column, whether or not it worked out.
                break;
            }
            aboveY = supportY;
            above = support;
        }
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::villageGolemSpawner
