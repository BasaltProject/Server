#include "BrainInternals.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/Aabb.hpp>
#include <basalt/server/runtime/entity/BlockRay.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <utility>

namespace basalt::server::runtime::entity::villager {
namespace {

/// parity: `Attributes.FOLLOW_RANGE` of a villager, sixteen, and the pathfinder it sizes.
constexpr double kFollowRange = 16.0;
constexpr float kPathLength = 48.0F;
constexpr std::int32_t kPathNodes = 768;
constexpr double kSneakVisibility = 0.8;
constexpr double kMinVisibleRange = 2.0;
constexpr double kPlayerEye = 1.62;
constexpr double kSneakingEye = 1.27;

/// parity: each sensor's scan rate, in the order `Villager.BRAIN_PROVIDER` lists them.
enum Sensor : std::size_t {
    kLiving,
    kPlayers,
    kItems,
    kBed,
    kHurtBy,
    kHostiles,
    kBabies,
    kSecondaryPois,
    kGolem,
    kSensorCount,
};

constexpr std::array<std::int32_t, kSensorCount> kScanRate{20, 20, 20, 20, 20, 20, 20, 40, 200};

/// parity: `VillagerHostilesSensor.ACCEPTABLE_DISTANCE_FROM_HOSTILES`.
[[nodiscard]] std::optional<float> hostileDistance(std::string_view type) {
    static constexpr std::array<std::pair<std::string_view, float>, 11> kHostiles{{{"drowned", 8.0F},
                                                                                   {"evoker", 12.0F},
                                                                                   {"husk", 8.0F},
                                                                                   {"illusioner", 12.0F},
                                                                                   {"pillager", 15.0F},
                                                                                   {"ravager", 12.0F},
                                                                                   {"vex", 8.0F},
                                                                                   {"vindicator", 10.0F},
                                                                                   {"zoglin", 10.0F},
                                                                                   {"zombie", 8.0F},
                                                                                   {"zombie_villager", 8.0F}}};
    for (const auto& [name, distance] : kHostiles) {
        if (name == type) {
            return distance;
        }
    }
    return std::nullopt;
}

[[nodiscard]] double eyeY(const ServerEntity& mob) { return mob.y + mobBreathing::eyeHeightOf(mob); }

/// parity: `LivingEntity.hasLineOfSight`, eye to eye through what collides.
[[nodiscard]] bool lineOfSight(Ctx& ctx, double x, double y, double z) {
    const double eye = eyeY(ctx.self);
    const double dx = x - ctx.self.x;
    const double dy = y - eye;
    const double dz = z - ctx.self.z;
    if ((dx * dx) + (dy * dy) + (dz * dz) > 128.0 * 128.0) {
        return false;
    }
    return !blockRay::cast(ctx.mob.world, ctx.self.x, eye, ctx.self.z, x, y, z).has_value();
}

/// parity: `Sensor.TARGET_CONDITIONS`, the non-combat test at the follow range.
[[nodiscard]] bool targetable(Ctx& ctx, std::int32_t id) {
    if (const ai::PlayerView* view = look::player(ctx, id); view != nullptr) {
        if (view->spectator) {
            return false;
        }
        const double range = std::max(kFollowRange * (view->sneaking ? kSneakVisibility : 1.0), kMinVisibleRange);
        if (ctx.mob.distanceSquaredTo(view->x, view->y, view->z) > range * range) {
            return false;
        }
        return lineOfSight(ctx, view->x, view->y + (view->sneaking ? kSneakingEye : kPlayerEye), view->z);
    }
    const ServerEntity* found = look::mob(ctx, id);
    if (found == nullptr || !look::alive(ctx, id)) {
        return false;
    }
    if (ctx.mob.distanceSquaredTo(found->x, found->y, found->z) > kFollowRange * kFollowRange) {
        return false;
    }
    return lineOfSight(ctx, found->x, eyeY(*found), found->z);
}

/// parity: `NearestLivingEntitySensor`: everything alive in the box grown by the follow range.
void senseLiving(Ctx& ctx) {
    const Aabb box =
        Aabb::of(ctx.self.x, ctx.self.y, ctx.self.z, ctx.self.width(), ctx.self.height()).inflate(kFollowRange);
    std::vector<std::pair<double, std::int32_t>> found;
    for (ServerEntity* candidate : ctx.mob.entities) {
        if (candidate == nullptr || candidate == &ctx.self || candidate->removed || candidate->dying ||
            !mobs::isMob(candidate->type())) {
            continue;
        }
        if (!Aabb::of(candidate->x, candidate->y, candidate->z, candidate->width(), candidate->height())
                 .intersects(box)) {
            continue;
        }
        found.emplace_back(ctx.mob.distanceSquaredTo(candidate->x, candidate->y, candidate->z), candidate->entityId());
    }
    for (const ai::PlayerView& player : ctx.mob.players) {
        if (Aabb::of(player.x, player.y, player.z, player.width, player.height).intersects(box)) {
            found.emplace_back(ctx.mob.distanceSquaredTo(player.x, player.y, player.z), player.entityId);
        }
    }
    std::ranges::stable_sort(found, {}, &std::pair<double, std::int32_t>::first);
    std::vector<std::int32_t> nearest;
    std::vector<std::int32_t> visible;
    for (const auto& [distance, id] : found) {
        nearest.push_back(id);
        if (targetable(ctx, id)) {
            visible.push_back(id);
        }
    }
    // parity: an empty list is stored as no memory at all.
    if (nearest.empty()) {
        ctx.mem.nearestLivingEntities.erase();
    } else {
        ctx.mem.nearestLivingEntities.set(std::move(nearest));
    }
    ctx.mem.visibleLivingEntities.set(std::move(visible));
}

/// parity: `PlayerSensor`, the nearest players and the nearest one it can see.
void sensePlayers(Ctx& ctx) {
    std::vector<std::pair<double, std::int32_t>> found;
    for (const ai::PlayerView& player : ctx.mob.players) {
        const double distance = ctx.mob.distanceSquaredTo(player.x, player.y, player.z);
        if (!player.spectator && distance < kFollowRange * kFollowRange) {
            found.emplace_back(distance, player.entityId);
        }
    }
    std::ranges::stable_sort(found, {}, &std::pair<double, std::int32_t>::first);
    std::vector<std::int32_t> players;
    std::optional<std::int32_t> visible;
    for (const auto& [distance, id] : found) {
        players.push_back(id);
        if (!visible.has_value() && targetable(ctx, id)) {
            visible = id;
        }
    }
    if (players.empty()) {
        ctx.mem.nearestPlayers.erase();
    } else {
        ctx.mem.nearestPlayers.set(std::move(players));
    }
    if (visible.has_value()) {
        ctx.mem.nearestVisiblePlayer.set(*visible);
    } else {
        ctx.mem.nearestVisiblePlayer.erase();
    }
}

/// parity: `NearestItemSensor`, the nearest wanted item it can see within thirty-two blocks.
void senseItems(Ctx& ctx) {
    constexpr double kHorizontal = 32.0;
    constexpr double kVertical = 16.0;
    constexpr double kItemEye = 0.2125;
    std::vector<VillagerWorld::ItemView> items;
    ctx.world.itemsNear(ctx.self, kHorizontal, kVertical, items);
    std::ranges::stable_sort(items, {}, [&ctx](const VillagerWorld::ItemView& item) {
        return ctx.mob.distanceSquaredTo(item.x, item.y, item.z);
    });
    for (const VillagerWorld::ItemView& item : items) {
        if (villagerData::wantsToPickUp(ctx.self, ctx.state, item.stack) &&
            ctx.mob.distanceSquaredTo(item.x, item.y, item.z) < kHorizontal * kHorizontal &&
            lineOfSight(ctx, item.x, item.y + kItemEye, item.z)) {
            ctx.mem.nearestVisibleWantedItem.set(item.id);
            return;
        }
    }
    ctx.mem.nearestVisibleWantedItem.erase();
}

/// parity: `NearestBedSensor`, a baby's look for a bed it can reach, five candidates at a time.
void senseBed(Ctx& ctx) {
    if (!look::isBaby(ctx.self)) {
        return;
    }
    constexpr std::int64_t kCacheTimeout = 40;
    constexpr std::int32_t kBatch = 5;
    VillagerState& state = ctx.state;
    state.bedTried = 0;
    state.bedLastUpdate = ctx.now + ctx.random.nextInt(20);
    std::vector<world::poi::PoiRecord> homes;
    ctx.poi.inRange(world::poi::maskOf(world::poi::PoiType::kHome), look::blockPosition(ctx.self), 48,
                    world::poi::Occupancy::kAny, homes);
    std::vector<std::array<std::int32_t, 3>> targets;
    for (const world::poi::PoiRecord& home : homes) {
        const std::int64_t key =
            (static_cast<std::int64_t>(home.pos.x) << 38) ^ (static_cast<std::int64_t>(home.pos.z) << 12) ^ home.pos.y;
        if (std::ranges::any_of(state.bedCache, [key](const auto& entry) { return entry.first == key; })) {
            continue;
        }
        if (++state.bedTried >= kBatch) {
            continue;
        }
        state.bedCache.emplace_back(key, state.bedLastUpdate + kCacheTimeout);
        targets.push_back({home.pos.x, home.pos.y, home.pos.z});
    }
    const BlockPos at = look::pathStart(ctx, ctx.self);
    const std::optional<pathFinder::Route> route =
        targets.empty()
            ? std::nullopt
            : pathFinder::findRoute(
                  ctx.mob.world, at.x, at.y, at.z, targets,
                  pathFinder::RouteOptions{
                      .accuracy = 1, .maxPathLength = kPathLength, .maxVisited = kPathNodes, .openDoors = true});
    if (route.has_value() && route->reached) {
        const BlockPos bed{route->target[0], route->target[1], route->target[2]};
        if (ctx.poi.type(bed).has_value()) {
            ctx.mem.nearestBed.set(bed);
        }
    } else if (state.bedTried < kBatch) {
        std::erase_if(state.bedCache, [&state](const auto& entry) { return entry.second < state.bedLastUpdate; });
    }
}

/// parity: `HurtBySensor`, the damage of the last two seconds and whoever dealt it.
void senseHurtBy(Ctx& ctx) {
    if (ctx.self.panicTicks > 0) {
        ctx.mem.hurtBy.set(ctx.self.lastDamageCause);
        if (ctx.self.hurtBy.has_value() && look::alive(ctx, *ctx.self.hurtBy)) {
            ctx.mem.hurtByEntity.set(*ctx.self.hurtBy);
        }
    } else {
        ctx.mem.hurtBy.erase();
    }
    if (ctx.mem.hurtByEntity.present() && !look::alive(ctx, *ctx.mem.hurtByEntity.value)) {
        ctx.mem.hurtByEntity.erase();
    }
}

/// parity: `VillagerHostilesSensor`.
void senseHostiles(Ctx& ctx) {
    if (ctx.mem.visibleLivingEntities.present()) {
        for (const std::int32_t id : *ctx.mem.visibleLivingEntities.value) {
            const ServerEntity* found = look::mob(ctx, id);
            if (found == nullptr) {
                continue;
            }
            const std::optional<float> distance = hostileDistance(found->type().path());
            if (distance.has_value() && look::distanceSqr(ctx, id) <= static_cast<double>(*distance * *distance)) {
                ctx.mem.nearestHostile.set(id);
                return;
            }
        }
    }
    ctx.mem.nearestHostile.erase();
}

/// parity: `VillagerBabiesSensor`.
void senseBabies(Ctx& ctx) {
    std::vector<std::int32_t> babies;
    if (ctx.mem.visibleLivingEntities.present()) {
        for (const std::int32_t id : *ctx.mem.visibleLivingEntities.value) {
            const ServerEntity* found = look::mob(ctx, id);
            if (found != nullptr && found->type().path() == "villager" && look::isBaby(*found)) {
                babies.push_back(id);
            }
        }
    }
    if (babies.empty()) {
        ctx.mem.visibleVillagerBabies.erase();
    } else {
        ctx.mem.visibleVillagerBabies.set(std::move(babies));
    }
}

/// parity: `SecondaryPoiSensor`, the farmland around a farmer.
void senseSecondaryPois(Ctx& ctx) {
    std::vector<BlockPos> found;
    if (ctx.self.profession == "farmer") {
        const BlockPos center = look::blockPosition(ctx.self);
        for (std::int32_t dx = -4; dx <= 4; ++dx) {
            for (std::int32_t dy = -2; dy <= 2; ++dy) {
                for (std::int32_t dz = -4; dz <= 4; ++dz) {
                    const std::optional<std::int32_t> state =
                        ctx.mob.world.blockState(center.x + dx, center.y + dy, center.z + dz);
                    const wire::common::Identifier* block =
                        state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
                    if (block != nullptr && block->path() == "farmland") {
                        found.push_back(BlockPos{center.x + dx, center.y + dy, center.z + dz});
                    }
                }
            }
        }
    }
    if (found.empty()) {
        ctx.mem.secondaryJobSite.erase();
    } else {
        ctx.mem.secondaryJobSite.set(std::move(found));
    }
}

/// parity: `GolemSensor.checkForNearbyGolem`.
void senseGolem(Ctx& ctx) {
    if (!ctx.mem.nearestLivingEntities.present()) {
        return;
    }
    for (const std::int32_t id : *ctx.mem.nearestLivingEntities.value) {
        const ServerEntity* found = look::mob(ctx, id);
        if (found != nullptr && found->type().path() == "iron_golem") {
            villagerBrain::golemDetected(ctx.self);
            return;
        }
    }
}

} // namespace

namespace sensors {

void tick(Ctx& ctx) {
    VillagerState& state = ctx.state;
    if (!state.sensorsStarted) {
        // parity: `Sensor.randomlyDelayStart`, each sensor's first scan somewhere in its period.
        for (std::size_t index = 0; index < kSensorCount; ++index) {
            state.sensorClock[index] = ctx.random.nextInt(kScanRate[index]);
        }
        state.sensorsStarted = true;
    }
    for (std::size_t index = 0; index < kSensorCount; ++index) {
        if (--state.sensorClock[index] > 0) {
            continue;
        }
        state.sensorClock[index] = kScanRate[index];
        switch (static_cast<Sensor>(index)) {
        case kLiving:
            senseLiving(ctx);
            break;
        case kPlayers:
            sensePlayers(ctx);
            break;
        case kItems:
            senseItems(ctx);
            break;
        case kBed:
            senseBed(ctx);
            break;
        case kHurtBy:
            senseHurtBy(ctx);
            break;
        case kHostiles:
            senseHostiles(ctx);
            break;
        case kBabies:
            senseBabies(ctx);
            break;
        case kSecondaryPois:
            senseSecondaryPois(ctx);
            break;
        case kGolem:
            senseGolem(ctx);
            break;
        case kSensorCount:
            break;
        }
    }
}

} // namespace sensors

} // namespace basalt::server::runtime::entity::villager
