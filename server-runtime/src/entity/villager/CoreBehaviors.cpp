#include "BrainInternals.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/PathFinder.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace basalt::server::runtime::entity::villager {
namespace {

constexpr float kPathLength = 48.0F;
constexpr std::int32_t kPathNodes = 768;
constexpr double kHalfPi = static_cast<double>(static_cast<float>(std::numbers::pi) / 2.0F);

[[nodiscard]] std::string_view blockPath(Ctx& ctx, const BlockPos& pos) {
    const std::optional<std::int32_t> state = ctx.mob.world.blockState(pos.x, pos.y, pos.z);
    const wire::common::Identifier* block =
        state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
    return block == nullptr ? std::string_view{} : std::string_view{block->path()};
}

[[nodiscard]] bool isOpenDoor(Ctx& ctx, const BlockPos& pos, bool& isDoor) {
    const std::optional<std::int32_t> state = ctx.mob.world.blockState(pos.x, pos.y, pos.z);
    isDoor = state.has_value() && pathFinder::isMobDoor(*state);
    if (!isDoor) {
        return false;
    }
    const std::map<std::string, std::string> properties = server::registry::blockStateDefinitions::propertiesOf(*state);
    const auto open = properties.find("open");
    return open != properties.end() && open->second == "true";
}

// --- Swim -----------------------------------------------------------------------------------------

bool swimWanted(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    // parity: `isInFluidDeeperThan(getFluidJumpThreshold())` or lava.
    return ai::inWater(ctx.mob.world, ctx.self) || blockPath(ctx, look::blockPosition(ctx.self)) == "lava";
}

void swimTick(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.random.nextFloat() < node.speed) {
        livingMovement::jump(ctx.mob.world, ctx.self);
    }
}

// --- InteractWithDoor ---------------------------------------------------------------------------

/// parity: `isMobComingThroughDoor` for a neighbour of the same kind.
[[nodiscard]] bool comingThrough(const ServerEntity& other, const BlockPos& door) {
    if (!other.path.has_value() || other.pathIndex <= 0 ||
        other.pathIndex >= static_cast<std::int32_t>(other.path->size())) {
        return false;
    }
    const auto& from = (*other.path)[static_cast<std::size_t>(other.pathIndex - 1)];
    const auto& to = (*other.path)[static_cast<std::size_t>(other.pathIndex)];
    return (from[0] == door.x && from[1] == door.y && from[2] == door.z) ||
           (to[0] == door.x && to[1] == door.y && to[2] == door.z);
}

/// parity: `InteractWithDoor.closeDoorsThatIHaveOpenedOrPassedThrough`.
void closeDoors(Ctx& ctx, const std::optional<BlockPos>& from, const std::optional<BlockPos>& to) {
    if (!ctx.mem.doorsToClose.present()) {
        return;
    }
    std::vector<BlockPos>& doors = *ctx.mem.doorsToClose.value;
    for (auto it = doors.begin(); it != doors.end();) {
        const BlockPos door = *it;
        if ((from.has_value() && *from == door) || (to.has_value() && *to == door)) {
            ++it;
            continue;
        }
        bool isDoor = false;
        const bool open = isOpenDoor(ctx, door, isDoor);
        bool others = false;
        if (isDoor && open && look::closerToCenterThan(door, ctx.self, 3.0) &&
            ctx.mem.nearestLivingEntities.present()) {
            for (const std::int32_t id : *ctx.mem.nearestLivingEntities.value) {
                const ServerEntity* other = look::mob(ctx, id);
                if (other != nullptr && other->type() == ctx.self.type() &&
                    look::closerToCenterThan(door, *other, 2.0) && comingThrough(*other, door)) {
                    others = true;
                    break;
                }
            }
        }
        if (isDoor && open && look::closerToCenterThan(door, ctx.self, 3.0) && !others) {
            ctx.world.setDoorOpen(ctx.self, door, false);
        }
        it = doors.erase(it);
    }
}

bool doorTrigger(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    const ServerEntity& self = ctx.self;
    if (!ctx.mem.path.present() || !self.path.has_value() || self.pathIndex <= 0 ||
        self.pathIndex >= static_cast<std::int32_t>(self.path->size())) {
        return false;
    }
    const auto& nextNode = (*self.path)[static_cast<std::size_t>(self.pathIndex)];
    const BlockPos to{nextNode[0], nextNode[1], nextNode[2]};
    // parity: twenty ticks before the same node is looked at again.
    if (state.pos.has_value() && *state.pos == to) {
        state.i0 = 20;
    } else if (--state.i0 > 0) {
        return false;
    }
    state.pos = to;
    const auto& previous = (*self.path)[static_cast<std::size_t>(self.pathIndex - 1)];
    const BlockPos from{previous[0], previous[1], previous[2]};
    const auto remember = [&ctx](const BlockPos& door) {
        if (!ctx.mem.doorsToClose.present()) {
            ctx.mem.doorsToClose.set({});
        }
        std::vector<BlockPos>& doors = *ctx.mem.doorsToClose.value;
        if (std::ranges::find(doors, door) == doors.end()) {
            doors.push_back(door);
        }
    };
    bool isDoor = false;
    if (!isOpenDoor(ctx, from, isDoor) && isDoor) {
        ctx.world.setDoorOpen(ctx.self, from, true);
    }
    if (isDoor) {
        remember(from);
    }
    if (!isOpenDoor(ctx, to, isDoor) && isDoor) {
        ctx.world.setDoorOpen(ctx.self, to, true);
        remember(to);
    }
    closeDoors(ctx, from, to);
    return true;
}

// --- LookAtTargetSink ---------------------------------------------------------------------------

bool lookCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) { return ctx.mem.lookTarget.present(); }

bool lookStill(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.lookTarget.present()) {
        return false;
    }
    const Tracker& target = *ctx.mem.lookTarget.value;
    // parity: `EntityTracker.isVisibleBy`: alive and among the ones it can see.
    return target.entityId == 0 || (look::alive(ctx, target.entityId) && ctx.mem.visible(target.entityId));
}

void lookTick(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.lookTarget.present()) {
        return;
    }
    if (const std::optional<std::array<double, 3>> at = look::position(ctx, *ctx.mem.lookTarget.value);
        at.has_value()) {
        ai::lookControl::lookAt(ctx.self, (*at)[0], (*at)[1], (*at)[2]);
    }
}

void lookStop(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) { ctx.mem.lookTarget.erase(); }

// --- VillagerPanicTrigger -----------------------------------------------------------------------

[[nodiscard]] bool scared(const Ctx& ctx) { return ctx.mem.hurtBy.present() || ctx.mem.nearestHostile.present(); }

void panicStart(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!scared(ctx)) {
        return;
    }
    if (!brain::isActive(ctx.state, Activity::kPanic)) {
        ctx.mem.path.erase();
        ctx.mem.walkTarget.erase();
        ctx.mem.lookTarget.erase();
        ctx.mem.breedTarget.erase();
        ctx.mem.interactionTarget.erase();
    }
    brain::setActiveActivityIfPossible(ctx, Activity::kPanic);
}

bool panicStill(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) { return scared(ctx); }

void panicTick(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (ctx.now % 100 == 0) {
        brain::spawnGolemIfNeeded(ctx, 3);
    }
}

// --- WakeUp, ReactToBell, SetRaidStatus, UpdateActivityFromSchedule ------------------------------

bool wakeUp(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (brain::isActive(ctx.state, Activity::kRest) || !ctx.self.sleeping) {
        return false;
    }
    ctx.world.setSleeping(ctx.self, std::nullopt);
    villagerBrain::wokeUp(ctx.self, ctx.now);
    return true;
}

bool reactToBell(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.heardBellTime.present()) {
        return false;
    }
    if (!ctx.world.raidAt(ctx.self).has_value()) {
        brain::setActiveActivityIfPossible(ctx, Activity::kHide);
    }
    return true;
}

bool setRaidStatus(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (ctx.random.nextInt(20) != 0) {
        return false;
    }
    if (const std::optional<RaidView> raid = ctx.world.raidAt(ctx.self); raid.has_value()) {
        const Activity activity = raid->firstWaveSpawned && !raid->betweenWaves ? Activity::kRaid : Activity::kPreRaid;
        ctx.state.defaultActivity = activity;
        brain::setActiveActivityIfPossible(ctx, activity);
    }
    return true;
}

bool updateSchedule(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    brain::updateActivityFromSchedule(ctx);
    return true;
}

// --- MoveToTargetSink ---------------------------------------------------------------------------

[[nodiscard]] bool reached(Ctx& ctx, const WalkTarget& target) {
    const std::optional<BlockPos> at = look::block(ctx, target.target);
    return at.has_value() && look::manhattan(*at, look::blockPosition(ctx.self)) <= target.closeEnough;
}

/// parity: `MoveToTargetSink.tryComputePath`, a partial path accepted, and a step towards the target
/// when there is none at all.
[[nodiscard]] bool computePath(Ctx& ctx, NodeState& state, const WalkTarget& target) {
    const std::optional<BlockPos> goal = look::block(ctx, target.target);
    if (!goal.has_value()) {
        return false;
    }
    NodeExtra& extra = extraOf(state);
    extra.positions.clear();
    state.i1 = std::bit_cast<std::int32_t>(target.speed);
    const BlockPos from = look::pathStart(ctx, ctx.self);
    const pathFinder::RouteOptions options{
        .accuracy = 0, .maxPathLength = kPathLength, .maxVisited = kPathNodes, .openDoors = true};
    const std::array<std::array<std::int32_t, 3>, 1> goals{{{goal->x, goal->y, goal->z}}};
    std::optional<pathFinder::Route> route =
        pathFinder::findRoute(ctx.mob.world, from.x, from.y, from.z, goals, options);
    if (reached(ctx, target)) {
        ctx.mem.cantReachWalkTargetSince.erase();
    } else {
        if (route.has_value() && route->reached) {
            ctx.mem.cantReachWalkTargetSince.erase();
        } else if (!ctx.mem.cantReachWalkTargetSince.present()) {
            ctx.mem.cantReachWalkTargetSince.set(ctx.now);
        }
        if (!route.has_value()) {
            const std::optional<ai::randomPos::Spot> step =
                ai::randomPos::defaultPosTowards(ctx.mob.world, ctx.self, ctx.random, 10, 7,
                                                 {goal->x + 0.5, static_cast<double>(goal->y), goal->z + 0.5}, kHalfPi);
            if (step.has_value()) {
                const std::array<std::array<std::int32_t, 3>, 1> stepGoal{
                    {{static_cast<std::int32_t>(std::floor((*step)[0])),
                      static_cast<std::int32_t>(std::floor((*step)[1])),
                      static_cast<std::int32_t>(std::floor((*step)[2]))}}};
                route = pathFinder::findRoute(ctx.mob.world, from.x, from.y, from.z, stepGoal, options);
            }
        }
    }
    if (!route.has_value()) {
        return false;
    }
    extra.positions.reserve(route->nodes.size());
    for (const auto& node : route->nodes) {
        extra.positions.push_back(BlockPos{node[0], node[1], node[2]});
    }
    state.pos = *goal;
    return true;
}

void moveFollow(Ctx& ctx, NodeState& state) {
    NodeExtra& extra = extraOf(state);
    std::vector<std::array<std::int32_t, 3>> nodes;
    nodes.reserve(extra.positions.size());
    for (const BlockPos& pos : extra.positions) {
        nodes.push_back({pos.x, pos.y, pos.z});
    }
    const BlockPos goal = state.pos.value_or(look::blockPosition(ctx.self));
    ctx.mem.path.set(true);
    const bool moving =
        ai::navigation::follow(ctx.self, std::move(nodes), {goal.x + 0.5, static_cast<double>(goal.y), goal.z + 0.5},
                               static_cast<double>(std::bit_cast<float>(state.i1)));
    if (!moving) {
        ai::navigation::stop(ctx.self);
    }
}

bool moveCheck(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (ctx.mem.path.present() || !ctx.mem.walkTarget.present()) {
        return false;
    }
    if (state.i0 > 0) {
        --state.i0;
        return false;
    }
    const WalkTarget target = *ctx.mem.walkTarget.value;
    const bool done = reached(ctx, target);
    if (!done && computePath(ctx, state, target)) {
        return true;
    }
    ctx.mem.walkTarget.erase();
    if (done) {
        ctx.mem.cantReachWalkTargetSince.erase();
    }
    return false;
}

void moveStart(Ctx& ctx, const Node& /*node*/, NodeState& state) { moveFollow(ctx, state); }

bool moveStill(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (!state.pos.has_value() || !ctx.mem.walkTarget.present() || ai::navigation::isDone(ctx.self)) {
        return false;
    }
    const WalkTarget& target = *ctx.mem.walkTarget.value;
    const bool spectator = target.target.entityId != 0 && look::player(ctx, target.target.entityId) != nullptr &&
                           look::player(ctx, target.target.entityId)->spectator;
    return !reached(ctx, target) && !spectator;
}

void moveTick(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    if (!ctx.mem.walkTarget.present() || !state.pos.has_value()) {
        return;
    }
    const WalkTarget target = *ctx.mem.walkTarget.value;
    const std::optional<BlockPos> now = look::block(ctx, target.target);
    if (!now.has_value()) {
        return;
    }
    const auto dx = static_cast<double>(now->x - state.pos->x);
    const auto dy = static_cast<double>(now->y - state.pos->y);
    const auto dz = static_cast<double>(now->z - state.pos->z);
    // parity: a target more than two blocks from where the path was made for gets a new one.
    if ((dx * dx) + (dy * dy) + (dz * dz) > 4.0 && computePath(ctx, state, target)) {
        moveFollow(ctx, state);
    }
}

void moveStop(Ctx& ctx, const Node& /*node*/, NodeState& state) {
    ai::navigation::stop(ctx.self);
    ctx.mem.walkTarget.erase();
    ctx.mem.path.erase();
    extraOf(state).positions.clear();
    state.pos.reset();
}

// --- LookAndFollowTradingPlayerSink -------------------------------------------------------------

bool followCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    const std::optional<std::int32_t> trader = ctx.state.tradingPlayer;
    return trader.has_value() && look::player(ctx, *trader) != nullptr && !ai::inWater(ctx.mob.world, ctx.self) &&
           ctx.self.panicTicks <= 0 && look::distanceSqr(ctx, *trader) <= 16.0;
}

void followTrader(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    const std::int32_t trader = *ctx.state.tradingPlayer;
    ctx.mem.walkTarget.set(
        WalkTarget{.target = Tracker::ofEntity(trader, false), .speed = node.speed, .closeEnough = 2});
    ctx.mem.lookTarget.set(Tracker::ofEntity(trader, true));
}

void followStop(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    ctx.mem.walkTarget.erase();
    ctx.mem.lookTarget.erase();
}

// --- GoToWantedItem -------------------------------------------------------------------------------

bool goToWantedItem(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.walkTarget.present() || !ctx.mem.nearestVisibleWantedItem.present()) {
        return false;
    }
    std::vector<VillagerWorld::ItemView> items;
    ctx.world.itemsNear(ctx.self, node.a, node.a, items);
    const std::int32_t wanted = *ctx.mem.nearestVisibleWantedItem.value;
    const auto item = std::ranges::find(items, wanted, &VillagerWorld::ItemView::id);
    if (item == items.end() ||
        ctx.mob.distanceSquaredTo(item->x, item->y, item->z) >= static_cast<double>(node.a * node.a)) {
        return false;
    }
    const Tracker spot = Tracker::ofSpot({item->x, item->y, item->z});
    ctx.mem.lookTarget.set(spot);
    ctx.mem.walkTarget.set(WalkTarget{.target = spot, .speed = node.speed, .closeEnough = 0});
    return true;
}

constexpr Ops kSwim{.check = swimWanted, .canStillUse = swimWanted, .tick = swimTick};
constexpr Ops kDoor{.trigger = doorTrigger};
constexpr Ops kLook{.check = lookCheck, .canStillUse = lookStill, .tick = lookTick, .stop = lookStop};
constexpr Ops kPanic{.start = panicStart, .canStillUse = panicStill, .tick = panicTick};
constexpr Ops kWakeUp{.trigger = wakeUp};
constexpr Ops kBell{.trigger = reactToBell};
constexpr Ops kRaidStatus{.trigger = setRaidStatus};
constexpr Ops kSchedule{.trigger = updateSchedule};
constexpr Ops kMove{
    .check = moveCheck, .start = moveStart, .canStillUse = moveStill, .tick = moveTick, .stop = moveStop};
constexpr Ops kFollow{.check = followCheck,
                      .start = followTrader,
                      .canStillUse = followCheck,
                      .tick = followTrader,
                      .stop = followStop,
                      .neverTimesOut = true};
constexpr Ops kWantedItem{.trigger = goToWantedItem};

} // namespace

const Ops* ops::core(Kind kind) {
    switch (kind) {
    case Kind::kSwim:
        return &kSwim;
    case Kind::kInteractWithDoor:
        return &kDoor;
    case Kind::kLookAtTargetSink:
        return &kLook;
    case Kind::kPanicTrigger:
        return &kPanic;
    case Kind::kWakeUp:
        return &kWakeUp;
    case Kind::kReactToBell:
        return &kBell;
    case Kind::kSetRaidStatus:
        return &kRaidStatus;
    case Kind::kUpdateSchedule:
        return &kSchedule;
    case Kind::kMoveToTargetSink:
        return &kMove;
    case Kind::kFollowTradingPlayer:
        return &kFollow;
    case Kind::kGoToWantedItem:
        return &kWantedItem;
    default:
        return nullptr;
    }
}

} // namespace basalt::server::runtime::entity::villager
