#include "BrainInternals.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numbers>

namespace basalt::server::runtime::entity::villager {
namespace {

namespace poiTypes = world::poi::poiTypes;
using world::poi::Occupancy;
using world::poi::PoiMask;
using world::poi::PoiRecord;
using world::poi::PoiType;

constexpr float kPathLength = 48.0F;
constexpr std::int32_t kPathNodes = 768;
constexpr std::int32_t kScanRange = 48;
constexpr double kHalfPi = static_cast<double>(static_cast<float>(std::numbers::pi) / 2.0F);

/// Which predicate a node reads, as the table's `a` names it.
[[nodiscard]] PoiMask maskFor(const Ctx& ctx, const Node& node) {
    constexpr std::int32_t kHeld = 1;
    constexpr std::int32_t kAcquirable = 2;
    const std::string& profession = ctx.self.profession;
    if (node.a == kHeld) {
        // parity: `VillagerProfession.heldJobSite`.
        const PoiType site = poiTypes::jobSiteOf(profession);
        return site == PoiType::kNone ? 0 : world::poi::maskOf(site);
    }
    if (node.a == kAcquirable) {
        // parity: `acquirableJobSite`, every workstation for the unemployed, none for a nitwit.
        if (profession == "none" || profession.empty()) {
            return poiTypes::kAcquirableJobSites;
        }
        const PoiType site = poiTypes::jobSiteOf(profession);
        return site == PoiType::kNone ? 0 : world::poi::maskOf(site);
    }
    return node.poi;
}

[[nodiscard]] std::map<std::string, std::string> propertiesAt(Ctx& ctx, const BlockPos& pos, std::string& block) {
    const std::optional<std::int32_t> state = ctx.mob.world.blockState(pos.x, pos.y, pos.z);
    if (!state.has_value()) {
        return {};
    }
    const wire::common::Identifier* id = server::registry::blockStateDefinitions::blockOf(*state);
    block = id == nullptr ? std::string{} : id->path();
    return server::registry::blockStateDefinitions::propertiesOf(*state);
}

/// parity: `BlockTags.VILLAGERS_CAN_SLEEP_ON_BED` and the bed's `occupied`.
[[nodiscard]] bool bedAt(Ctx& ctx, const BlockPos& pos, bool& occupied) {
    std::string block;
    const std::map<std::string, std::string> properties = propertiesAt(ctx, pos, block);
    const auto flag = properties.find("occupied");
    occupied = flag != properties.end() && flag->second == "true";
    return block.ends_with("_bed");
}

[[nodiscard]] std::int64_t packed(const BlockPos& pos) {
    return ((static_cast<std::int64_t>(pos.x) & 0x3FFFFFFLL) << 38) |
           ((static_cast<std::int64_t>(pos.z) & 0x3FFFFFFLL) << 12) | (static_cast<std::int64_t>(pos.y) & 0xFFFLL);
}

[[nodiscard]] std::optional<pathFinder::Route>
routeTo(Ctx& ctx, ServerEntity& mob, const std::vector<std::array<std::int32_t, 3>>& targets, std::int32_t accuracy) {
    if (targets.empty()) {
        return std::nullopt;
    }
    const BlockPos from = look::pathStart(ctx, mob);
    return pathFinder::findRoute(
        ctx.mob.world, from.x, from.y, from.z, targets,
        pathFinder::RouteOptions{
            .accuracy = accuracy, .maxPathLength = kPathLength, .maxVisited = kPathNodes, .openDoors = true});
}

/// parity: `AcquirePoi.findPathToPois`, reach range the widest of the types.
[[nodiscard]] std::optional<pathFinder::Route> routeToPois(Ctx& ctx, const std::vector<PoiRecord>& pois) {
    std::vector<std::array<std::int32_t, 3>> targets;
    std::int32_t range = 1;
    for (const PoiRecord& poi : pois) {
        range = std::max(range, poiTypes::validRange(poi.type));
        targets.push_back({poi.pos.x, poi.pos.y, poi.pos.z});
    }
    return routeTo(ctx, ctx.self, targets, range);
}

[[nodiscard]] double distSqr(const BlockPos& one, const BlockPos& other) {
    const auto dx = static_cast<double>(one.x - other.x);
    const auto dy = static_cast<double>(one.y - other.y);
    const auto dz = static_cast<double>(one.z - other.z);
    return (dx * dx) + (dy * dy) + (dz * dz);
}

// --- ValidateNearbyPoi --------------------------------------------------------------------------

bool validateNearbyPoi(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    Memory<BlockPos>* memory = ctx.mem.position(node.memory);
    if (memory == nullptr || !memory->present()) {
        return false;
    }
    const BlockPos pos = *memory->value;
    if (!look::closerToCenterThan(pos, ctx.self, 16.0)) {
        return false;
    }
    if (!ctx.poi.exists(pos, maskFor(ctx, node))) {
        memory->erase();
        return true;
    }
    bool occupied = false;
    if (bedAt(ctx, pos, occupied) && occupied && !ctx.self.sleeping) {
        memory->erase();
        // parity: `bedIsOccupiedByVillager`, somebody asleep in that very block.
        const bool villagerAsleep = std::ranges::any_of(ctx.mob.entities, [&pos](const ServerEntity* other) {
            return other != nullptr && !other->removed && other->sleeping && other->type().path() == "villager" &&
                   static_cast<std::int32_t>(std::floor(other->x)) == pos.x &&
                   static_cast<std::int32_t>(std::floor(other->y)) == pos.y &&
                   static_cast<std::int32_t>(std::floor(other->z)) == pos.z;
        });
        if (!villagerAsleep) {
            static_cast<void>(ctx.poi.release(pos));
        }
    }
    return true;
}

// --- PoiCompetitorScan ----------------------------------------------------------------------------

bool poiCompetitorScan(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.jobSite.present() || !ctx.mem.nearestLivingEntities.present()) {
        return false;
    }
    const BlockPos pos = *ctx.mem.jobSite.value;
    const std::optional<PoiType> type = ctx.poi.type(pos);
    if (!type.has_value()) {
        return true;
    }
    ServerEntity* winner = &ctx.self;
    for (const std::int32_t id : *ctx.mem.nearestLivingEntities.value) {
        ServerEntity* other = look::mob(ctx, id);
        VillagerState* theirs = look::villagerState(ctx, id);
        if (other == nullptr || theirs == nullptr || other == &ctx.self || !look::alive(ctx, id) ||
            !theirs->memories.jobSite.present() || *theirs->memories.jobSite.value != pos ||
            poiTypes::jobSiteOf(other->profession) != *type) {
            continue;
        }
        // parity: `selectWinner`, the one with more experience keeps the site.
        ServerEntity* loser = winner->villagerExperience > other->villagerExperience ? other : winner;
        winner = loser == other ? winner : other;
        villagerBrain::stateOf(*loser).memories.jobSite.erase();
    }
    return true;
}

// --- AcquirePoi -----------------------------------------------------------------------------------

void markAttempt(Ctx& ctx, NodeExtra::Retry& retry) {
    retry.previous = ctx.now;
    const std::int32_t suggested = retry.delay + ctx.random.nextInt(40) + 40;
    retry.delay = std::min(suggested, 400);
    retry.next = ctx.now + retry.delay;
}

bool acquirePoi(Ctx& ctx, const Node& node, NodeState& state) {
    Memory<BlockPos>* acquire = ctx.mem.position(node.memory);
    Memory<BlockPos>* validate = ctx.mem.position(node.memory2);
    if (acquire == nullptr || acquire->present() || (validate != nullptr && validate->present())) {
        return false;
    }
    if (node.flag && look::isBaby(ctx.self)) {
        return false;
    }
    if (state.t0 == 0) {
        state.t0 = ctx.now + ctx.random.nextInt(20);
        return false;
    }
    if (ctx.now < state.t0) {
        return false;
    }
    state.t0 = ctx.now + 20 + ctx.random.nextInt(20);
    NodeExtra& extra = extraOf(state);
    std::erase_if(extra.retries, [&ctx](const auto& entry) { return ctx.now - entry.second.previous >= 400; });
    const PoiMask mask = maskFor(ctx, node);
    std::vector<PoiRecord> found;
    ctx.poi.inRange(mask, look::blockPosition(ctx.self), kScanRange, Occupancy::kHasSpace, found);
    // parity: the retry cache is asked about every candidate before the nearest five are kept.
    std::vector<PoiRecord> candidates;
    for (const PoiRecord& record : found) {
        const std::int64_t key = packed(record.pos);
        const auto retry = std::ranges::find(extra.retries, key, &std::pair<std::int64_t, NodeExtra::Retry>::first);
        if (retry != extra.retries.end()) {
            if (ctx.now < retry->second.next) {
                continue;
            }
            markAttempt(ctx, retry->second);
        }
        candidates.push_back(record);
    }
    const BlockPos here = look::blockPosition(ctx.self);
    std::ranges::stable_sort(candidates, {}, [&here](const PoiRecord& record) { return distSqr(record.pos, here); });
    if (candidates.size() > 5) {
        candidates.resize(5);
    }
    if (node.b == 1) {
        std::erase_if(candidates, [&ctx](const PoiRecord& record) {
            bool occupied = false;
            return !bedAt(ctx, record.pos, occupied) || occupied;
        });
    }
    const std::optional<pathFinder::Route> route = routeToPois(ctx, candidates);
    if (route.has_value() && route->reached) {
        const BlockPos target{route->target[0], route->target[1], route->target[2]};
        if (ctx.poi.type(target).has_value()) {
            // parity: `PoiManager.take` within one block of the target, which is the target itself.
            std::vector<PoiRecord> near;
            ctx.poi.inRange(mask, target, 1, Occupancy::kHasSpace, near);
            if (std::ranges::any_of(near, [&target](const PoiRecord& record) { return record.pos == target; })) {
                static_cast<void>(ctx.poi.acquire(target));
            }
            acquire->set(target);
            if (node.event >= 0) {
                ctx.world.entityEvent(ctx.self, node.event);
            }
            extra.retries.clear();
        }
    } else {
        for (const PoiRecord& record : candidates) {
            const std::int64_t key = packed(record.pos);
            if (std::ranges::find(extra.retries, key, &std::pair<std::int64_t, NodeExtra::Retry>::first) ==
                extra.retries.end()) {
                NodeExtra::Retry retry;
                markAttempt(ctx, retry);
                extra.retries.emplace_back(key, retry);
            }
        }
    }
    return true;
}

// --- GoToPotentialJobSite -----------------------------------------------------------------------

bool potentialCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.potentialJobSite.present()) {
        return false;
    }
    const std::optional<Activity> activity = brain::activeNonCore(ctx.state);
    return !activity.has_value() || *activity == Activity::kIdle || *activity == Activity::kWork ||
           *activity == Activity::kPlay;
}

bool potentialStill(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) { return ctx.mem.potentialJobSite.present(); }

void potentialTick(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    look::walkAndLook(ctx, Tracker::ofBlock(*ctx.mem.potentialJobSite.value), node.speed, 1);
}

void potentialStop(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (ctx.mem.potentialJobSite.present()) {
        const BlockPos pos = *ctx.mem.potentialJobSite.value;
        if (ctx.poi.type(pos).has_value()) {
            static_cast<void>(ctx.poi.release(pos));
        }
    }
    ctx.mem.potentialJobSite.erase();
}

// --- YieldJobSite ---------------------------------------------------------------------------------

bool yieldJobSite(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (!ctx.mem.potentialJobSite.present() || ctx.mem.jobSite.present() || !ctx.mem.nearestLivingEntities.present()) {
        return false;
    }
    if (look::isBaby(ctx.self) || (ctx.self.profession != "none" && !ctx.self.profession.empty())) {
        return false;
    }
    const BlockPos pos = *ctx.mem.potentialJobSite.value;
    const std::optional<PoiType> type = ctx.poi.type(pos);
    if (!type.has_value()) {
        return true;
    }
    for (const std::int32_t id : *ctx.mem.nearestLivingEntities.value) {
        ServerEntity* other = look::mob(ctx, id);
        VillagerState* theirs = look::villagerState(ctx, id);
        if (other == nullptr || theirs == nullptr || other == &ctx.self || !look::alive(ctx, id) ||
            theirs->memories.potentialJobSite.present() || poiTypes::jobSiteOf(other->profession) != *type) {
            continue;
        }
        bool wants = false;
        if (theirs->memories.jobSite.present()) {
            wants = *theirs->memories.jobSite.value == pos;
        } else {
            const std::optional<pathFinder::Route> route =
                routeTo(ctx, *other, {{pos.x, pos.y, pos.z}}, poiTypes::validRange(*type));
            wants = route.has_value() && route->reached;
        }
        if (!wants) {
            continue;
        }
        ctx.mem.walkTarget.erase();
        ctx.mem.lookTarget.erase();
        ctx.mem.potentialJobSite.erase();
        if (!theirs->memories.jobSite.present()) {
            const Tracker spot = Tracker::ofBlock(pos);
            theirs->memories.lookTarget.set(spot);
            theirs->memories.walkTarget.set(WalkTarget{.target = spot, .speed = node.speed, .closeEnough = 1});
            theirs->memories.potentialJobSite.set(pos);
        }
        break;
    }
    return true;
}

// --- AssignProfessionFromJobSite and ResetProfession -------------------------------------------

bool assignProfession(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.potentialJobSite.present()) {
        return false;
    }
    const BlockPos pos = *ctx.mem.potentialJobSite.value;
    if (!look::closerToCenterThan(pos, ctx.self, 2.0)) {
        return false;
    }
    ctx.mem.potentialJobSite.erase();
    ctx.mem.jobSite.set(pos);
    ctx.world.entityEvent(ctx.self, 14);
    if (ctx.self.profession != "none" && !ctx.self.profession.empty()) {
        return true;
    }
    if (const std::optional<PoiType> type = ctx.poi.type(pos); type.has_value()) {
        const std::string_view profession = poiTypes::professionAt(*type);
        if (!profession.empty()) {
            villagerData::setProfession(ctx, profession);
        }
    }
    return true;
}

bool resetProfession(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (ctx.mem.jobSite.present()) {
        return false;
    }
    const bool canBeFired = villagerData::hasJob(ctx.self);
    if (canBeFired && ctx.self.villagerExperience == 0 && ctx.self.villagerLevel <= 1) {
        villagerData::setProfession(ctx, "none");
        return true;
    }
    return false;
}

// --- SetWalkTargetFromBlockMemory ---------------------------------------------------------------

bool walkToBlockMemory(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    Memory<BlockPos>* memory = ctx.mem.position(node.memory);
    if (memory == nullptr || !memory->present() || ctx.mem.walkTarget.present()) {
        return false;
    }
    const BlockPos pos = *memory->value;
    const BlockPos here = look::blockPosition(ctx.self);
    const bool givenUp =
        ctx.mem.cantReachWalkTargetSince.present() && ctx.now - *ctx.mem.cantReachWalkTargetSince.value > node.c;
    if (givenUp) {
        brain::releasePoi(ctx, node.memory);
        memory->erase();
        ctx.mem.cantReachWalkTargetSince.set(ctx.now);
        return true;
    }
    if (look::manhattan(pos, here) > node.b) {
        std::optional<ai::randomPos::Spot> towards;
        for (std::int32_t tries = 1;; ++tries) {
            towards = ai::randomPos::defaultPosTowards(ctx.mob.world, ctx.self, ctx.random, 15, 7,
                                                       {pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}, kHalfPi);
            if (towards.has_value() && look::manhattan(BlockPos{static_cast<std::int32_t>(std::floor((*towards)[0])),
                                                                static_cast<std::int32_t>(std::floor((*towards)[1])),
                                                                static_cast<std::int32_t>(std::floor((*towards)[2]))},
                                                       here) <= node.b) {
                break;
            }
            if (tries == 1000) {
                brain::releasePoi(ctx, node.memory);
                memory->erase();
                ctx.mem.cantReachWalkTargetSince.set(ctx.now);
                return true;
            }
        }
        ctx.mem.walkTarget.set(
            WalkTarget{.target = Tracker::ofSpot(*towards), .speed = node.speed, .closeEnough = node.a});
    } else if (look::manhattan(pos, here) > node.a) {
        ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofBlock(pos), .speed = node.speed, .closeEnough = node.a});
    }
    return true;
}

// --- StrollAroundPoi, StrollToPoi, StrollToPoiList ---------------------------------------------

bool strollAroundPoi(Ctx& ctx, const Node& node, NodeState& state) {
    Memory<BlockPos>* memory = ctx.mem.position(node.memory);
    if (memory == nullptr || !memory->present() ||
        !look::closerToCenterThan(*memory->value, ctx.self, static_cast<double>(node.a))) {
        return false;
    }
    if (ctx.now <= state.t0) {
        return true;
    }
    const std::optional<ai::randomPos::Spot> spot = ai::randomPos::landPos(ctx.mob.world, ctx.self, ctx.random, 8, 6);
    if (spot.has_value()) {
        ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofSpot(*spot), .speed = node.speed, .closeEnough = 1});
    } else {
        ctx.mem.walkTarget.erase();
    }
    state.t0 = ctx.now + 180;
    return true;
}

bool strollToPoi(Ctx& ctx, const Node& node, NodeState& state) {
    Memory<BlockPos>* memory = ctx.mem.position(node.memory);
    if (memory == nullptr || !memory->present() ||
        !look::closerToCenterThan(*memory->value, ctx.self, static_cast<double>(node.b))) {
        return false;
    }
    if (ctx.now <= state.t0) {
        return true;
    }
    ctx.mem.walkTarget.set(
        WalkTarget{.target = Tracker::ofBlock(*memory->value), .speed = node.speed, .closeEnough = node.a});
    state.t0 = ctx.now + 80;
    return true;
}

bool strollToPoiList(Ctx& ctx, const Node& node, NodeState& state) {
    if (!ctx.mem.secondaryJobSite.present() || !ctx.mem.jobSite.present()) {
        return false;
    }
    const std::vector<BlockPos>& list = *ctx.mem.secondaryJobSite.value;
    if (list.empty()) {
        return false;
    }
    const BlockPos target = list[static_cast<std::size_t>(ctx.random.nextInt(static_cast<std::int32_t>(list.size())))];
    if (!look::closerToCenterThan(*ctx.mem.jobSite.value, ctx.self, static_cast<double>(node.b))) {
        return false;
    }
    if (ctx.now > state.t0) {
        ctx.mem.walkTarget.set(
            WalkTarget{.target = Tracker::ofBlock(target), .speed = node.speed, .closeEnough = node.a});
        state.t0 = ctx.now + 100;
    }
    return true;
}

// --- SetClosestHomeAsWalkTarget -----------------------------------------------------------------

bool closestHome(Ctx& ctx, const Node& node, NodeState& state) {
    if (ctx.mem.walkTarget.present() || ctx.mem.home.present()) {
        return false;
    }
    if (ctx.now - state.t0 < 20) {
        return false;
    }
    const BlockPos here = look::blockPosition(ctx.self);
    const std::optional<BlockPos> closest =
        ctx.poi.findClosest(world::poi::maskOf(PoiType::kHome), here, kScanRange, Occupancy::kAny);
    if (!closest.has_value() || distSqr(*closest, here) <= 4.0) {
        return false;
    }
    std::int32_t tried = 0;
    state.t0 = ctx.now + ctx.random.nextInt(20);
    NodeExtra& extra = extraOf(state);
    std::vector<PoiRecord> found;
    ctx.poi.inRange(world::poi::maskOf(PoiType::kHome), here, kScanRange, Occupancy::kAny, found);
    std::vector<PoiRecord> chosen;
    for (const PoiRecord& record : found) {
        const std::int64_t key = packed(record.pos);
        if (std::ranges::find(extra.batch, key, &std::pair<std::int64_t, std::int64_t>::first) != extra.batch.end()) {
            continue;
        }
        if (++tried >= 5) {
            continue;
        }
        extra.batch.emplace_back(key, state.t0 + 40);
        chosen.push_back(record);
    }
    const std::optional<pathFinder::Route> route = routeToPois(ctx, chosen);
    if (route.has_value() && route->reached) {
        const BlockPos target{route->target[0], route->target[1], route->target[2]};
        if (ctx.poi.type(target).has_value()) {
            ctx.mem.walkTarget.set(
                WalkTarget{.target = Tracker::ofBlock(target), .speed = node.speed, .closeEnough = 1});
        }
    } else if (tried < 5) {
        std::erase_if(extra.batch, [&state](const auto& entry) { return entry.second < state.t0; });
    }
    return true;
}

// --- LocateHidingPlace ----------------------------------------------------------------------------

bool locateHidingPlace(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.walkTarget.present()) {
        return false;
    }
    const BlockPos here = look::blockPosition(ctx.self);
    const PoiMask homes = world::poi::maskOf(PoiType::kHome);
    std::vector<PoiRecord> found;
    std::optional<BlockPos> place;
    ctx.poi.inRange(homes, here, node.b + 1, Occupancy::kAny, found);
    if (!found.empty() && look::closerToCenterThan(found.front().pos, ctx.self, static_cast<double>(node.b))) {
        place = found.front().pos;
    }
    if (!place.has_value()) {
        // parity: `PoiManager.getRandom` over `Util.toShuffledList`.
        ctx.poi.inRange(homes, here, node.a, Occupancy::kAny, found);
        for (auto size = static_cast<std::int32_t>(found.size()); size > 1; --size) {
            const std::int32_t swapTo = ctx.random.nextInt(size);
            std::swap(found[static_cast<std::size_t>(size - 1)], found[static_cast<std::size_t>(swapTo)]);
        }
        if (!found.empty()) {
            place = found.front().pos;
        }
    }
    if (!place.has_value() && ctx.mem.home.present()) {
        place = *ctx.mem.home.value;
    }
    if (place.has_value()) {
        ctx.mem.path.erase();
        ctx.mem.lookTarget.erase();
        ctx.mem.breedTarget.erase();
        ctx.mem.interactionTarget.erase();
        ctx.mem.hidingPlace.set(*place);
        if (!look::closerToCenterThan(*place, ctx.self, static_cast<double>(node.b))) {
            ctx.mem.walkTarget.set(
                WalkTarget{.target = Tracker::ofBlock(*place), .speed = node.speed, .closeEnough = node.b});
        }
    }
    return true;
}

// --- VillageBoundRandomStroll and GoToClosestVillage --------------------------------------------

[[nodiscard]] std::int32_t sectionsToVillage(Ctx& ctx, const BlockPos& pos) {
    return ctx.poi.sectionsToVillage(pos.x >> 4, pos.y >> 4, pos.z >> 4);
}

bool villageBoundStroll(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.walkTarget.present()) {
        return false;
    }
    const BlockPos here = look::blockPosition(ctx.self);
    std::optional<ai::randomPos::Spot> spot;
    if (ctx.poi.isVillage(here)) {
        spot = ai::randomPos::landPos(ctx.mob.world, ctx.self, ctx.random, node.a, node.b);
    } else {
        // parity: `BehaviorUtils.findSectionClosestToVillage`, the nearest better section in a
        // five-section cube.
        const std::int32_t sectionX = here.x >> 4;
        const std::int32_t sectionY = here.y >> 4;
        const std::int32_t sectionZ = here.z >> 4;
        const std::int32_t current = ctx.poi.sectionsToVillage(sectionX, sectionY, sectionZ);
        std::optional<std::array<std::int32_t, 3>> best;
        std::int32_t bestDistance = current;
        // parity: `SectionPos.cube` walks x fastest, then y, then z, and the first nearest wins.
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            for (std::int32_t dy = -2; dy <= 2; ++dy) {
                for (std::int32_t dx = -2; dx <= 2; ++dx) {
                    const std::int32_t distance =
                        ctx.poi.sectionsToVillage(sectionX + dx, sectionY + dy, sectionZ + dz);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        best = std::array<std::int32_t, 3>{sectionX + dx, sectionY + dy, sectionZ + dz};
                    }
                }
            }
        }
        if (best.has_value()) {
            // parity: `Vec3.atBottomCenterOf(SectionPos.center())`.
            const ai::randomPos::Spot towards{((*best)[0] << 4) + 8.5, static_cast<double>(((*best)[1] << 4) + 8),
                                              ((*best)[2] << 4) + 8.5};
            spot =
                ai::randomPos::defaultPosTowards(ctx.mob.world, ctx.self, ctx.random, node.a, node.b, towards, kHalfPi);
        } else {
            spot = ai::randomPos::landPos(ctx.mob.world, ctx.self, ctx.random, node.a, node.b);
        }
    }
    if (spot.has_value()) {
        ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofSpot(*spot), .speed = node.speed, .closeEnough = 0});
    } else {
        ctx.mem.walkTarget.erase();
    }
    return true;
}

bool goToClosestVillage(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.walkTarget.present()) {
        return false;
    }
    const BlockPos here = look::blockPosition(ctx.self);
    if (ctx.poi.isVillage(here)) {
        return false;
    }
    const std::int32_t current = sectionsToVillage(ctx, here);
    std::optional<ai::randomPos::Spot> target;
    for (std::int32_t attempt = 0; attempt < 5; ++attempt) {
        const std::optional<ai::randomPos::Spot> spot = ai::randomPos::landPos(
            ctx.mob.world, ctx.self, ctx.random, 15, 7, [&ctx](std::int32_t x, std::int32_t y, std::int32_t z) {
                return -static_cast<double>(ctx.poi.sectionsToVillage(x >> 4, y >> 4, z >> 4));
            });
        if (!spot.has_value()) {
            continue;
        }
        const std::int32_t there = sectionsToVillage(ctx, BlockPos{static_cast<std::int32_t>(std::floor((*spot)[0])),
                                                                   static_cast<std::int32_t>(std::floor((*spot)[1])),
                                                                   static_cast<std::int32_t>(std::floor((*spot)[2]))});
        if (there < current) {
            target = spot;
            break;
        }
        if (there == current) {
            target = spot;
        }
    }
    if (target.has_value()) {
        ctx.mem.walkTarget.set(
            WalkTarget{.target = Tracker::ofSpot(*target), .speed = node.speed, .closeEnough = node.a});
    }
    return true;
}

constexpr Ops kValidate{.trigger = validateNearbyPoi};
constexpr Ops kCompetitor{.trigger = poiCompetitorScan};
constexpr Ops kAcquire{.trigger = acquirePoi};
constexpr Ops kPotential{
    .check = potentialCheck, .canStillUse = potentialStill, .tick = potentialTick, .stop = potentialStop};
constexpr Ops kYield{.trigger = yieldJobSite};
constexpr Ops kAssign{.trigger = assignProfession};
constexpr Ops kReset{.trigger = resetProfession};
constexpr Ops kBlockMemory{.trigger = walkToBlockMemory};
constexpr Ops kStrollAround{.trigger = strollAroundPoi};
constexpr Ops kStrollTo{.trigger = strollToPoi};
constexpr Ops kStrollList{.trigger = strollToPoiList};
constexpr Ops kClosestHome{.trigger = closestHome};
constexpr Ops kHiding{.trigger = locateHidingPlace};
constexpr Ops kVillageStroll{.trigger = villageBoundStroll};
constexpr Ops kClosestVillage{.trigger = goToClosestVillage};

} // namespace

const Ops* ops::poi(Kind kind) {
    switch (kind) {
    case Kind::kValidateNearbyPoi:
        return &kValidate;
    case Kind::kPoiCompetitorScan:
        return &kCompetitor;
    case Kind::kAcquirePoi:
        return &kAcquire;
    case Kind::kGoToPotentialJobSite:
        return &kPotential;
    case Kind::kYieldJobSite:
        return &kYield;
    case Kind::kAssignProfession:
        return &kAssign;
    case Kind::kResetProfession:
        return &kReset;
    case Kind::kWalkToBlockMemory:
        return &kBlockMemory;
    case Kind::kStrollAroundPoi:
        return &kStrollAround;
    case Kind::kStrollToPoi:
        return &kStrollTo;
    case Kind::kStrollToPoiList:
        return &kStrollList;
    case Kind::kClosestHomeAsWalkTarget:
        return &kClosestHome;
    case Kind::kLocateHidingPlace:
        return &kHiding;
    case Kind::kVillageBoundStroll:
        return &kVillageStroll;
    case Kind::kGoToClosestVillage:
        return &kClosestVillage;
    default:
        return nullptr;
    }
}

} // namespace basalt::server::runtime::entity::villager
