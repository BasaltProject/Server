#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/runtime/entity/EntityCollision.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>
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

constexpr std::int64_t kRecentSleepTicks = 24000;
constexpr std::int32_t kGolemMemoryTicks = 599;
constexpr std::int32_t kGolemSensorInterval = 200;
constexpr double kGolemSensorRangeSquared = 16.0 * 16.0;
constexpr std::int64_t kPanicInterval = 100;
constexpr std::int64_t kGossipInterval = 1200;
constexpr double kVillagerSearch = 10.0;
constexpr std::int32_t kPanicAgreement = 3;
constexpr std::int32_t kGossipAgreement = 5;
constexpr std::int32_t kSpawnAttempts = 10;
constexpr std::int32_t kSpawnRangeXz = 8;
constexpr std::int32_t kSpawnRangeY = 6;
constexpr double kThreatRangeSquared = 8.0 * 8.0;

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

[[nodiscard]] const std::set<std::string, std::less<>>& villagerThreats() {
    static const std::set<std::string, std::less<>> all{"zombie",   "husk",       "drowned", "zombie_villager",
                                                        "pillager", "vindicator", "evoker",  "ravager",
                                                        "vex",      "illusioner", "zoglin"};
    return all;
}

[[nodiscard]] double distanceSquared(const ServerEntity& first, const ServerEntity& second) {
    const double dx = first.x - second.x;
    const double dy = first.y - second.y;
    const double dz = first.z - second.z;
    return dx * dx + dy * dy + dz * dz;
}

[[nodiscard]] std::vector<ServerEntity*> nearbyVillagers(const ServerEntity& source,
                                                         const std::vector<ServerEntity*>& living) {
    const Aabb sourceBox = Aabb::of(source.x, source.y, source.z, source.width(), source.height());
    const Aabb search{sourceBox.minX - kVillagerSearch, sourceBox.minY - kVillagerSearch,
                      sourceBox.minZ - kVillagerSearch, sourceBox.maxX + kVillagerSearch,
                      sourceBox.maxY + kVillagerSearch, sourceBox.maxZ + kVillagerSearch};
    std::vector<ServerEntity*> found;
    for (ServerEntity* candidate : living) {
        if (candidate == nullptr || candidate->removed || candidate->type().path() != "villager") {
            continue;
        }
        if (Aabb::of(candidate->x, candidate->y, candidate->z, candidate->width(), candidate->height())
                .intersects(search)) {
            found.push_back(candidate);
        }
    }
    return found;
}

[[nodiscard]] bool nearbyGolem(const ServerEntity& villager, const std::vector<ServerEntity*>& living) {
    return std::ranges::any_of(living, [&villager](const ServerEntity* candidate) {
        return candidate != nullptr && !candidate->removed && candidate->type() == ironGolem() &&
               distanceSquared(villager, *candidate) <= kGolemSensorRangeSquared;
    });
}

[[nodiscard]] bool nearbyThreat(const ServerEntity& villager, const std::vector<ServerEntity*>& living) {
    return std::ranges::any_of(living, [&villager](const ServerEntity* candidate) {
        return candidate != nullptr && !candidate->removed && villagerThreats().contains(candidate->type().path()) &&
               distanceSquared(villager, *candidate) <= kThreatRangeSquared;
    });
}

/// parity: `SpawnUtil.Strategy.LEGACY_IRON_GOLEM`, which is fussier than an ordinary spawn check.
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

bool wantsGolem(const ServerEntity& villager, std::int64_t gameTime) {
    return villager.lastSleptGameTime != std::numeric_limits<std::int64_t>::min() &&
           gameTime - villager.lastSleptGameTime < kRecentSleepTicks && villager.golemDetectedTicks <= 0;
}

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

void tick(LoadedWorld& world, ServerEntity& villager, const std::vector<ServerEntity*>& living,
          random::RandomSource& random, const Spawn& spawn) {
    if (villager.golemDetectedTicks > 0) {
        villager.golemDetectedTicks--;
    }
    if ((villager.tickCount + villager.entityId()) % kGolemSensorInterval == 0 && nearbyGolem(villager, living)) {
        villager.golemDetectedTicks = kGolemMemoryTicks;
    }
    if (!wantsGolem(villager, world.gameTime())) {
        return;
    }

    const bool panicking = villager.panicTicks > 0 || nearbyThreat(villager, living);
    const bool gossipReady = villager.lastGossipTime == std::numeric_limits<std::int64_t>::min() ||
                             world.gameTime() >= villager.lastGossipTime + kGossipInterval;
    std::int32_t agreement = 0;
    if (panicking && world.gameTime() % kPanicInterval == 0) {
        agreement = kPanicAgreement;
    } else if (entity::ai::villagerSchedule::activityAt(world.clock().totalTicks()) ==
                   entity::ai::villagerSchedule::Activity::Meet &&
               gossipReady) {
        villager.lastGossipTime = world.gameTime();
        agreement = kGossipAgreement;
    } else {
        return;
    }

    const std::vector<ServerEntity*> nearby = nearbyVillagers(villager, living);
    std::int32_t agreeing = 0;
    for (const ServerEntity* candidate : nearby) {
        if (agreeing >= 5) {
            break;
        }
        if (wantsGolem(*candidate, world.gameTime())) {
            agreeing++;
        }
    }
    if (agreeing < agreement) {
        return;
    }
    const std::optional<std::array<std::int32_t, 3>> position = findSpawnPosition(world, villager, random);
    if (!position.has_value() || !spawn || !spawn(*position)) {
        return;
    }
    // Everyone who agreed has now seen one, which is what stops a village making a second.
    for (ServerEntity* candidate : nearby) {
        candidate->golemDetectedTicks = kGolemMemoryTicks;
    }
}

} // namespace basalt::server::runtime::world::villageGolemSpawner
