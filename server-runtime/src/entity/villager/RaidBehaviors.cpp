#include "BrainInternals.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/RandomPos.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::entity::villager {
namespace {

bool calmDown(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    const bool scared = ctx.mem.hurtBy.present() || ctx.mem.nearestHostile.present() ||
                        (ctx.mem.hurtByEntity.present() && look::distanceSqr(ctx, *ctx.mem.hurtByEntity.value) <= 36.0);
    if (!scared) {
        ctx.mem.hurtBy.erase();
        ctx.mem.hurtByEntity.erase();
        brain::updateActivityFromSchedule(ctx);
    }
    return true;
}

bool walkAwayFrom(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    Memory<std::int32_t>* memory = ctx.mem.entity(node.memory);
    if (memory == nullptr || !memory->present() || ctx.mem.walkTarget.present()) {
        return false;
    }
    const std::optional<std::array<double, 3>> avoid = look::feet(ctx, *memory->value);
    if (!avoid.has_value()) {
        return false;
    }
    const double distance = static_cast<double>(node.a);
    if (ctx.mob.distanceSquaredTo((*avoid)[0], (*avoid)[1], (*avoid)[2]) >= distance * distance) {
        return false;
    }
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const std::optional<ai::randomPos::Spot> flee =
            ai::randomPos::landPosAway(ctx.mob.world, ctx.self, ctx.random, 16, 7, *avoid);
        if (flee.has_value()) {
            ctx.mem.walkTarget.set(WalkTarget{.target = Tracker::ofSpot(*flee), .speed = node.speed, .closeEnough = 0});
            break;
        }
    }
    return true;
}

bool ringBell(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (!ctx.mem.meetingPoint.present()) {
        return false;
    }
    if (ctx.random.nextFloat() <= 0.95F) {
        return false;
    }
    const BlockPos pos = *ctx.mem.meetingPoint.value;
    const BlockPos here = look::blockPosition(ctx.self);
    const auto dx = static_cast<double>(pos.x - here.x);
    const auto dy = static_cast<double>(pos.y - here.y);
    const auto dz = static_cast<double>(pos.z - here.z);
    if ((dx * dx) + (dy * dy) + (dz * dz) < 9.0) {
        const std::optional<std::int32_t> state = ctx.mob.world.blockState(pos.x, pos.y, pos.z);
        const wire::common::Identifier* block =
            state.has_value() ? server::registry::blockStateDefinitions::blockOf(*state) : nullptr;
        if (block != nullptr && block->path() == "bell") {
            ctx.world.ringBell(ctx.self, pos);
        }
    }
    return true;
}

bool resetRaidStatus(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (ctx.random.nextInt(20) != 0) {
        return false;
    }
    const std::optional<RaidView> raid = ctx.world.raidAt(ctx.self);
    if (!raid.has_value() || raid->stopped || raid->loss) {
        ctx.state.defaultActivity = Activity::kIdle;
        brain::updateActivityFromSchedule(ctx);
    }
    return true;
}

/// parity: `MoveToSkySeeingSpot.hasNoBlocksAbove`.
[[nodiscard]] bool noBlocksAbove(Ctx& ctx, const BlockPos& pos) {
    return ctx.world.canSeeSky(ctx.self, pos) &&
           static_cast<double>(ctx.world.motionBlockingHeight(ctx.self, pos.x, pos.z)) <= ctx.self.y;
}

bool moveToSkySeeingSpot(Ctx& ctx, const Node& node, NodeState& /*state*/) {
    if (ctx.mem.walkTarget.present()) {
        return false;
    }
    const BlockPos here = look::blockPosition(ctx.self);
    if (ctx.world.canSeeSky(ctx.self, here)) {
        return false;
    }
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const BlockPos pos{here.x + ctx.random.nextInt(20) - 10, here.y + ctx.random.nextInt(6) - 3,
                           here.z + ctx.random.nextInt(20) - 10};
        if (noBlocksAbove(ctx, pos)) {
            ctx.mem.walkTarget.set(
                WalkTarget{.target = Tracker::ofSpot({pos.x + 0.5, static_cast<double>(pos.y), pos.z + 0.5}),
                           .speed = node.speed,
                           .closeEnough = 0});
            break;
        }
    }
    return true;
}

bool celebrateCheck(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    const std::optional<RaidView> raid = ctx.world.raidAt(ctx.self);
    return raid.has_value() && raid->victory && noBlocksAbove(ctx, look::blockPosition(ctx.self));
}

bool celebrateStill(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    const std::optional<RaidView> raid = ctx.world.raidAt(ctx.self);
    return raid.has_value() && !raid->stopped;
}

void celebrateStop(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) { brain::updateActivityFromSchedule(ctx); }

void celebrateTick(Ctx& ctx, const Node& /*node*/, NodeState& /*state*/) {
    if (ctx.random.nextInt(100) == 0) {
        ctx.world.sound(ctx.self, "entity.villager.celebrate", 1.0F, 1.0F);
    }
    if (ctx.random.nextInt(200) == 0 && noBlocksAbove(ctx, look::blockPosition(ctx.self))) {
        // parity: `Util.getRandom(DyeColor.values())` and a flight of up to two.
        const std::int32_t colour = ctx.random.nextInt(16);
        const std::int32_t flight = ctx.random.nextInt(3);
        ctx.world.launchFirework(ctx.self, colour, flight);
    }
}

bool setHiddenState(Ctx& ctx, const Node& node, NodeState& state) {
    if (!ctx.mem.hidingPlace.present() || !ctx.mem.heardBellTime.present()) {
        return false;
    }
    const std::int32_t stayHidden = node.a * 20;
    const bool timedOut = *ctx.mem.heardBellTime.value + 300 <= ctx.now;
    if (state.i0 <= stayHidden && !timedOut) {
        const BlockPos hide = *ctx.mem.hidingPlace.value;
        const BlockPos here = look::blockPosition(ctx.self);
        const auto dx = static_cast<double>(hide.x - here.x);
        const auto dy = static_cast<double>(hide.y - here.y);
        const auto dz = static_cast<double>(hide.z - here.z);
        if ((dx * dx) + (dy * dy) + (dz * dz) < static_cast<double>(node.b * node.b)) {
            ++state.i0;
        }
        return true;
    }
    ctx.mem.heardBellTime.erase();
    ctx.mem.hidingPlace.erase();
    brain::updateActivityFromSchedule(ctx);
    state.i0 = 0;
    return true;
}

constexpr Ops kCalmDown{.trigger = calmDown};
constexpr Ops kAway{.trigger = walkAwayFrom};
constexpr Ops kRingBell{.trigger = ringBell};
constexpr Ops kResetRaid{.trigger = resetRaidStatus};
constexpr Ops kSky{.trigger = moveToSkySeeingSpot};
constexpr Ops kCelebrate{
    .check = celebrateCheck, .canStillUse = celebrateStill, .tick = celebrateTick, .stop = celebrateStop};
constexpr Ops kHidden{.trigger = setHiddenState};

} // namespace

const Ops* ops::raid(Kind kind) {
    switch (kind) {
    case Kind::kCalmDown:
        return &kCalmDown;
    case Kind::kWalkAwayFrom:
        return &kAway;
    case Kind::kRingBell:
        return &kRingBell;
    case Kind::kResetRaidStatus:
        return &kResetRaid;
    case Kind::kMoveToSkySeeingSpot:
        return &kSky;
    case Kind::kCelebrate:
        return &kCelebrate;
    case Kind::kSetHiddenState:
        return &kHidden;
    default:
        return nullptr;
    }
}

} // namespace basalt::server::runtime::entity::villager
