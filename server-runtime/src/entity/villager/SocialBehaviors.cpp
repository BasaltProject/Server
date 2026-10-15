#include "BrainInternals.hpp"

#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>

namespace basalt::server::runtime::entity::villager {
namespace {

namespace definitions = server::registry::blockStateDefinitions;

[[nodiscard]] std::string_view pathAt(Ctx& ctx, const BlockPos& pos) {
    const std::optional<std::int32_t> state = ctx.mob.world.blockState(pos.x, pos.y, pos.z);
    const wire::common::Identifier* block = state.has_value() ? definitions::blockOf(*state) : nullptr;
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

/// parity: `BlockTags.BEDS`, which is also what babies jump on and villagers sleep in.
[[nodiscard]] bool isBed(Ctx& ctx, const BlockPos& pos) { return pathAt(ctx, pos).ends_with("_bed"); }

[[nodiscard]] bool bedOccupied(Ctx& ctx, const BlockPos& pos) {
    const std::optional<std::int32_t> state = ctx.mob.world.blockState(pos.x, pos.y, pos.z);
    if (!state.has_value()) {
        return false;
    }
    const std::map<std::string, std::string> properties = definitions::propertiesOf(*state);
    const auto occupied = properties.find("occupied");
    return occupied != properties.end() && occupied->second == "true";
}

bool interactWith(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    Memory<std::int32_t>* target = ctx.mem.entity(node.memory);
    if (target == nullptr || ctx.mem.walkTarget.present() || !ctx.mem.visibleLivingEntities.present()) {
        return false;
    }
    // parity: the breeding form asks `AgeableMob.canBreed` of both sides.
    if (node.flag && !villagerData::canBreed(ctx.self)) {
        return false;
    }
    const auto valid = [&ctx, &node](std::int32_t id) {
        if (!look::matches(ctx, id, node.filter)) {
            return false;
        }
        if (!node.flag) {
            return true;
        }
        const ServerEntity* other = look::mob(ctx, id);
        return other != nullptr && villagerData::canBreed(*other);
    };
    const std::vector<std::int32_t>& visible = *ctx.mem.visibleLivingEntities.value;
    if (std::ranges::none_of(visible, valid)) {
        return false;
    }
    const double rangeSqr = static_cast<double>(node.a * node.a);
    for (const std::int32_t id : visible) {
        if (look::distanceSqr(ctx, id) <= rangeSqr && valid(id)) {
            target->set(id);
            ctx.mem.lookTarget.set(Tracker::ofEntity(id, true));
            ctx.mem.walkTarget.set(
                WalkTarget{.target = Tracker::ofEntity(id, false), .speed = node.speed, .closeEnough = node.b});
            break;
        }
    }
    return true;
}

bool walkToLookTarget(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.walkTarget.present() || !ctx.mem.lookTarget.present()) {
        return false;
    }
    ctx.mem.walkTarget.set(WalkTarget{.target = *ctx.mem.lookTarget.value, .speed = node.speed, .closeEnough = node.a});
    return true;
}

bool setEntityLookTarget(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.lookTarget.present() || !ctx.mem.visibleLivingEntities.present()) {
        return false;
    }
    const auto maxDistance = static_cast<float>(node.a);
    const std::optional<std::int32_t> closest =
        look::closestVisible(ctx, node.filter, static_cast<double>(maxDistance * maxDistance));
    // parity: never one of its own passengers.
    if (!closest.has_value() || std::ranges::find(ctx.self.passengerIds, *closest) != ctx.self.passengerIds.end()) {
        return false;
    }
    ctx.mem.lookTarget.set(Tracker::ofEntity(*closest, true));
    return true;
}

bool socializeAtBell(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.meetingPoint.present() || !ctx.mem.visibleLivingEntities.present() ||
        ctx.mem.interactionTarget.present()) {
        return false;
    }
    if (ctx.random.nextInt(100) != 0 || !look::closerToCenterThan(*ctx.mem.meetingPoint.value, ctx.self, 4.0)) {
        return false;
    }
    const std::vector<std::int32_t>& visible = *ctx.mem.visibleLivingEntities.value;
    if (std::ranges::none_of(visible, [&ctx](std::int32_t id) { return look::matches(ctx, id, Filter::kVillager); })) {
        return false;
    }
    if (const std::optional<std::int32_t> other = look::closestVisible(ctx, Filter::kVillager, 32.0);
        other.has_value()) {
        ctx.mem.interactionTarget.set(*other);
        ctx.mem.lookTarget.set(Tracker::ofEntity(*other, true));
        ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofEntity(*other, false), .speed = 0.3F, .closeEnough = 1});
    }
    return true;
}

[[nodiscard]] bool onOrOverBed(Ctx& ctx) {
    const BlockPos at = look::blockPosition(ctx.self);
    return isBed(ctx, at) || isBed(ctx, BlockPos{at.x, at.y - 1, at.z});
}

bool jumpCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    return ctx.mem.nearestBed.present() && !ctx.mem.walkTarget.present() && look::isBaby(ctx.self) &&
           (onOrOverBed(ctx) || ctx.mem.nearestBed.present());
}

void jumpStart(Ctx& ctx, const Node& node, NodeState& state) {
    if (!ctx.mem.nearestBed.present()) {
        return;
    }
    state.pos = *ctx.mem.nearestBed.value;
    state.i0 = 100;
    state.i1 = 3 + ctx.random.nextInt(4);
    state.i2 = 0;
    ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofBlock(*state.pos), .speed = node.speed, .closeEnough = 0});
}

void jumpStop(Ctx& /*ctx*/, const Node& /*node*/, NodeState& state) {
    state.pos.reset();
    state.i0 = 0;
    state.i1 = 0;
    state.i2 = 0;
}

bool jumpStill(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (!look::isBaby(ctx.self) || !state.pos.has_value() || !isBed(ctx, *state.pos)) {
        return false;
    }
    const bool over = onOrOverBed(ctx);
    return !(!over && state.i0 <= 0) && !(over && state.i1 <= 0);
}

void jumpTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (!onOrOverBed(ctx)) {
        --state.i0;
    } else if (state.i2 > 0) {
        --state.i2;
    } else if (isBed(ctx, look::blockPosition(ctx.self))) {
        livingMovement::jump(ctx.mob.world, ctx.self);
        --state.i1;
        state.i2 = 5;
    }
}

[[nodiscard]] std::optional<std::int32_t> chasing(Ctx& ctx, std::int32_t kid) {
    const VillagerState* theirs = look::villagerState(ctx, kid);
    if (theirs == nullptr || !theirs->memories.interactionTarget.present()) {
        return std::nullopt;
    }
    return *theirs->memories.interactionTarget.value;
}

void chase(Ctx& ctx, std::int32_t kid) {
    ctx.mem.interactionTarget.set(kid);
    ctx.mem.lookTarget.set(Tracker::ofEntity(kid, true));
    ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofEntity(kid, false), .speed = 0.6F, .closeEnough = 1});
}

bool playTag(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.visibleVillagerBabies.present() || ctx.mem.walkTarget.present()) {
        return false;
    }
    if (ctx.random.nextInt(10) != 0) {
        return false;
    }
    const std::vector<std::int32_t>& friends = *ctx.mem.visibleVillagerBabies.value;
    const std::int32_t me = ctx.self.entityId();
    const bool chased = std::ranges::any_of(friends, [&ctx, me](std::int32_t kid) { return chasing(ctx, kid) == me; });
    if (!chased) {
        // parity: `findSomeoneBeingChased`, the kid with the fewest chasers, at most five.
        std::vector<std::pair<std::int32_t, std::int32_t>> counts;
        for (const std::int32_t kid : friends) {
            if (const std::optional<std::int32_t> target = chasing(ctx, kid); target.has_value()) {
                const auto found = std::ranges::find(counts, *target, &std::pair<std::int32_t, std::int32_t>::first);
                if (found == counts.end()) {
                    counts.emplace_back(*target, 1);
                } else {
                    ++found->second;
                }
            }
        }
        std::ranges::stable_sort(counts, {}, &std::pair<std::int32_t, std::int32_t>::second);
        for (const auto& [kid, count] : counts) {
            if (count > 0 && count <= 5) {
                chase(ctx, kid);
                return true;
            }
        }
        if (!friends.empty()) {
            chase(ctx, friends.front());
        }
        return true;
    }
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const std::optional<ai::randomPos::Spot> spot =
            ai::randomPos::landPos(ctx.mob.world, ctx.self, ctx.random, 20, 8);
        if (spot.has_value() && ctx.poi.isVillage(BlockPos{static_cast<std::int32_t>(std::floor((*spot)[0])),
                                                           static_cast<std::int32_t>(std::floor((*spot)[1])),
                                                           static_cast<std::int32_t>(std::floor((*spot)[2]))})) {
            ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofSpot(*spot), .speed = 0.6F, .closeEnough = 0});
            break;
        }
    }
    return true;
}

[[nodiscard]] ServerEntity* breedPartner(Ctx& ctx) {
    if (!ctx.mem.breedTarget.present()) {
        return nullptr;
    }
    const std::int32_t id = *ctx.mem.breedTarget.value;
    ServerEntity* other = look::mob(ctx, id);
    if (other == nullptr || other->type().path() != "villager" || !look::alive(ctx, id) || !ctx.mem.visible(id)) {
        return nullptr;
    }
    return villagerData::canBreed(ctx.self) && villagerData::canBreed(*other) ? other : nullptr;
}

bool loveCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    return ctx.mem.visibleLivingEntities.present() && breedPartner(ctx) != nullptr;
}

bool loveStill(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    return ctx.now <= state.t0 && breedPartner(ctx) != nullptr;
}

void loveStart(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ServerEntity* other = breedPartner(ctx);
    look::lockGaze(ctx, *other, 0.5F, 2);
    ctx.world.entityEvent(*other, 18);
    ctx.world.entityEvent(ctx.self, 18);
    state.t0 = ctx.now + 275 + ctx.random.nextInt(50);
}

/// parity: `VillagerMakeLove.tryToGiveBirth`.
void giveBirth(Ctx& ctx, ServerEntity& other) {
    std::vector<world::poi::PoiRecord> homes;
    ctx.poi.inRange(world::poi::maskOf(world::poi::PoiType::kHome), look::blockPosition(ctx.self), 48,
                    world::poi::Occupancy::kHasSpace, homes);
    std::optional<BlockPos> bed;
    const BlockPos from = look::pathStart(ctx, ctx.self);
    for (const world::poi::PoiRecord& home : homes) {
        const std::array<std::array<std::int32_t, 3>, 1> target{{{home.pos.x, home.pos.y, home.pos.z}}};
        const std::optional<pathFinder::Route> route = pathFinder::findRoute(
            ctx.mob.world, from.x, from.y, from.z, target,
            pathFinder::RouteOptions{.accuracy = 1, .maxPathLength = 48.0F, .maxVisited = 768, .openDoors = true});
        if (route.has_value() && route->reached) {
            bed = home.pos;
            static_cast<void>(ctx.poi.acquire(home.pos));
            break;
        }
    }
    if (!bed.has_value()) {
        ctx.world.entityEvent(other, 13);
        ctx.world.entityEvent(ctx.self, 13);
        return;
    }
    ServerEntity* child = ctx.world.spawnChild(ctx.self, other);
    if (child == nullptr) {
        static_cast<void>(ctx.poi.release(*bed));
        return;
    }
    ctx.self.age = 6000;
    other.age = 6000;
    child->age = -24000;
    ctx.world.entityEvent(*child, 12);
    villagerBrain::stateOf(*child).memories.home.set(*bed);
}

void loveTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ServerEntity* other = breedPartner(ctx);
    if (other == nullptr || look::distanceSqr(ctx, other->entityId()) > 5.0) {
        return;
    }
    look::lockGaze(ctx, *other, 0.5F, 2);
    if (ctx.now >= state.t0) {
        villagerData::eatAndDigest(ctx.state);
        villagerData::eatAndDigest(villagerBrain::stateOf(*other));
        giveBirth(ctx, *other);
    } else if (ctx.random.nextInt(35) == 0) {
        ctx.world.entityEvent(*other, 12);
        ctx.world.entityEvent(ctx.self, 12);
    }
}

void loveStop(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) { ctx.mem.breedTarget.erase(); }

bool sleepCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.home.present() || ctx.self.vehicleId.has_value()) {
        return false;
    }
    if (ctx.mem.lastWoken.present()) {
        const std::int64_t since = ctx.now - *ctx.mem.lastWoken.value;
        if (since > 0 && since < 100) {
            return false;
        }
    }
    const BlockPos bed = *ctx.mem.home.value;
    return look::closerToCenterThan(bed, ctx.self, 2.0) && isBed(ctx, bed) && !bedOccupied(ctx, bed);
}

bool sleepStill(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.home.present()) {
        return false;
    }
    const BlockPos bed = *ctx.mem.home.value;
    return brain::isActive(ctx.state, Activity::kRest) && ctx.self.y > bed.y + 0.4 &&
           look::closerToCenterThan(bed, ctx.self, 1.14);
}

void sleepStart(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (ctx.now <= state.t0) {
        return;
    }
    if (ctx.mem.doorsToClose.present()) {
        std::vector<BlockPos>& doors = *ctx.mem.doorsToClose.value;
        for (const BlockPos& door : doors) {
            const std::optional<std::int32_t> doorState = ctx.mob.world.blockState(door.x, door.y, door.z);
            if (!doorState.has_value() || !look::closerToCenterThan(door, ctx.self, 3.0)) {
                continue;
            }
            const std::map<std::string, std::string> properties = definitions::propertiesOf(*doorState);
            const auto open = properties.find("open");
            if (pathAt(ctx, door).ends_with("_door") && open != properties.end() && open->second == "true") {
                ctx.world.setDoorOpen(ctx.self, door, false);
            }
        }
        doors.clear();
    }
    ctx.world.setSleeping(ctx.self, *ctx.mem.home.value);
    ctx.mem.lastSlept.set(ctx.now);
    ctx.mem.walkTarget.erase();
    ctx.mem.cantReachWalkTargetSince.erase();
}

void sleepStop(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (ctx.self.sleeping) {
        ctx.world.setSleeping(ctx.self, std::nullopt);
        villagerBrain::wokeUp(ctx.self, ctx.now);
        state.t0 = ctx.now + 40;
    }
}

bool insideBrownianWalk(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.walkTarget.present()) {
        return false;
    }
    const BlockPos here = look::blockPosition(ctx.self);
    if (ctx.world.canSeeSky(ctx.self, here)) {
        return false;
    }
    std::vector<BlockPos> around;
    for (std::int32_t dz = -1; dz <= 1; ++dz) {
        for (std::int32_t dy = -1; dy <= 1; ++dy) {
            for (std::int32_t dx = -1; dx <= 1; ++dx) {
                around.push_back(BlockPos{here.x + dx, here.y + dy, here.z + dz});
            }
        }
    }
    // parity: `Collections.shuffle`, on its own unseeded random.
    for (auto size = static_cast<std::int32_t>(around.size()); size > 1; --size) {
        std::swap(around[static_cast<std::size_t>(size - 1)],
                  around[static_cast<std::size_t>(ctx.random.nextInt(size))]);
    }
    for (const BlockPos& pos : around) {
        const std::optional<std::int32_t> state = ctx.mob.world.blockState(pos.x, pos.y, pos.z);
        if (!state.has_value() || ctx.world.canSeeSky(ctx.self, pos) ||
            !server::registry::blockShapes::isCollisionFaceFull(*state, server::registry::BlockFace::Up)) {
            continue;
        }
        ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofBlock(pos), .speed = node.speed, .closeEnough = 0});
        break;
    }
    return true;
}

constexpr Ops kInteractWith{.trigger = interactWith};
constexpr Ops kWalkToLook{.trigger = walkToLookTarget};
constexpr Ops kLookTarget{.trigger = setEntityLookTarget};
constexpr Ops kSocialize{.trigger = socializeAtBell};
constexpr Ops kJump{.check = jumpCheck,
                    .start = jumpStart,
                    .canStillUse = jumpStill,
                    .tick = jumpTick,
                    .stop = jumpStop,
                    .neverTimesOut = true};
constexpr Ops kTag{.trigger = playTag};
constexpr Ops kLove{
    .check = loveCheck, .start = loveStart, .canStillUse = loveStill, .tick = loveTick, .stop = loveStop};
constexpr Ops kSleep{
    .check = sleepCheck, .start = sleepStart, .canStillUse = sleepStill, .stop = sleepStop, .neverTimesOut = true};
constexpr Ops kBrownian{.trigger = insideBrownianWalk};

} // namespace

const Ops* ops::social(Kind kind) {
    switch (kind) {
    case Kind::kInteractWith:
        return &kInteractWith;
    case Kind::kWalkToLookTarget:
        return &kWalkToLook;
    case Kind::kSetEntityLookTarget:
        return &kLookTarget;
    case Kind::kSocializeAtBell:
        return &kSocialize;
    case Kind::kJumpOnBed:
        return &kJump;
    case Kind::kPlayTag:
        return &kTag;
    case Kind::kMakeLove:
        return &kLove;
    case Kind::kSleepInBed:
        return &kSleep;
    case Kind::kInsideBrownianWalk:
        return &kBrownian;
    default:
        return nullptr;
    }
}

} // namespace basalt::server::runtime::entity::villager
