#include "BrainInternals.hpp"

#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/VillagerSchedule.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime::entity::villager {

VillagerState::VillagerState() = default;
VillagerState::VillagerState(VillagerState&&) noexcept = default;
VillagerState& VillagerState::operator=(VillagerState&&) noexcept = default;
VillagerState::~VillagerState() = default;

NodeExtra& extraOf(NodeState& state) {
    if (state.extra == nullptr) {
        state.extra = std::make_unique<NodeExtra>();
    }
    return *state.extra;
}

namespace {

[[nodiscard]] constexpr std::uint16_t bit(Activity activity) {
    return static_cast<std::uint16_t>(1U << static_cast<unsigned>(activity));
}

// The engine proper: `Behavior`, `OneShot`, `DoNothing`, `GateBehavior` and the trigger gate.
bool tryStart(Ctx& ctx, std::size_t index);
void tickOrStop(Ctx& ctx, std::size_t index);
void doStop(Ctx& ctx, std::size_t index);
bool trigger(Ctx& ctx, std::size_t index);

[[nodiscard]] const Node& nodeAt(const Ctx& ctx, std::size_t index) { return ctx.state.table->nodes[index]; }

[[nodiscard]] bool isGate(Kind kind) { return kind == Kind::kRunOne || kind == Kind::kGate; }

[[nodiscard]] bool isOneShot(Kind kind) {
    return kind == Kind::kTriggerGate || kind == Kind::kIfRaidWon || kind == Kind::kIfRaidOngoing ||
           ops::of(kind).trigger != nullptr;
}

/// parity: `ShufflingList.shuffle`: each entry gets `-random^(1/weight)` and the list is sorted by it.
void shuffle(Ctx& ctx, const Node& node, NodeState& state) {
    std::array<std::pair<double, std::uint8_t>, 12> keyed{};
    for (std::uint8_t child = 0; child < node.childCount; ++child) {
        const std::uint8_t at = state.order[child];
        const Node& entry = ctx.state.table->nodes[node.firstChild + at];
        const float roll = ctx.random.nextFloat();
        keyed[child] = {
            -std::pow(static_cast<double>(roll), static_cast<double>(1.0F / static_cast<float>(entry.weight))), at};
    }
    std::stable_sort(keyed.begin(), keyed.begin() + node.childCount,
                     [](const auto& one, const auto& other) { return one.first < other.first; });
    for (std::uint8_t child = 0; child < node.childCount; ++child) {
        state.order[child] = keyed[child].second;
    }
}

bool trigger(Ctx& ctx, std::size_t index) {
    const Node& node = nodeAt(ctx, index);
    NodeState& state = ctx.state.nodes[index];
    switch (node.kind) {
    case Kind::kTriggerGate: {
        shuffle(ctx, node, state);
        for (std::uint8_t child = 0; child < node.childCount; ++child) {
            if (trigger(ctx, node.firstChild + state.order[child])) {
                break;
            }
        }
        return true;
    }
    case Kind::kIfRaidWon: {
        const std::optional<RaidView> raid = ctx.world.raidAt(ctx.self);
        // parity: `raidExistsAndNotVictory`, which in fact asks for a victory.
        return raid.has_value() && raid->victory && trigger(ctx, node.firstChild);
    }
    case Kind::kIfRaidOngoing: {
        const std::optional<RaidView> raid = ctx.world.raidAt(ctx.self);
        return raid.has_value() && raid->active && !raid->victory && !raid->loss && trigger(ctx, node.firstChild);
    }
    default:
        return ops::of(node.kind).trigger(ctx, node, state);
    }
}

bool tryStart(Ctx& ctx, std::size_t index) {
    const Node& node = nodeAt(ctx, index);
    NodeState& state = ctx.state.nodes[index];
    if (isOneShot(node.kind)) {
        if (!trigger(ctx, index)) {
            return false;
        }
        state.running = true;
        return true;
    }
    if (node.kind == Kind::kDoNothing) {
        state.running = true;
        state.endTimestamp = ctx.now + node.minDuration + ctx.random.nextInt(node.maxDuration + 1 - node.minDuration);
        return true;
    }
    if (isGate(node.kind)) {
        if (node.requireAbsent != MemoryId::kNone && ctx.mem.present(node.requireAbsent)) {
            return false;
        }
        state.running = true;
        if (node.shuffled) {
            shuffle(ctx, node, state);
        }
        for (std::uint8_t child = 0; child < node.childCount; ++child) {
            const std::size_t at = node.firstChild + state.order[child];
            // parity: `RunningPolicy.RUN_ONE`, the first stopped child that starts.
            if (!ctx.state.nodes[at].running && tryStart(ctx, at)) {
                break;
            }
        }
        return true;
    }
    const Ops& behaviour = ops::of(node.kind);
    if (behaviour.check != nullptr && !behaviour.check(ctx, node, state)) {
        return false;
    }
    state.running = true;
    // parity: the duration is drawn from the level's random even when it cannot vary.
    const std::int64_t spread = static_cast<std::int64_t>(node.maxDuration) + 1 - node.minDuration;
    const auto bound = static_cast<std::int32_t>(static_cast<std::uint32_t>(spread));
    state.endTimestamp = ctx.now + node.minDuration + (bound > 0 ? ctx.random.nextInt(bound) : 0);
    if (behaviour.start != nullptr) {
        behaviour.start(ctx, node, state);
    }
    return true;
}

void doStop(Ctx& ctx, std::size_t index) {
    const Node& node = nodeAt(ctx, index);
    NodeState& state = ctx.state.nodes[index];
    state.running = false;
    if (isGate(node.kind)) {
        for (std::uint8_t child = 0; child < node.childCount; ++child) {
            const std::size_t at = node.firstChild + state.order[child];
            if (ctx.state.nodes[at].running) {
                doStop(ctx, at);
            }
        }
        if (node.eraseOnStop == MemoryId::kInteractionTarget) {
            ctx.mem.interactionTarget.erase();
        } else if (node.eraseOnStop == MemoryId::kBreedTarget) {
            ctx.mem.breedTarget.erase();
        }
        return;
    }
    if (isOneShot(node.kind) || node.kind == Kind::kDoNothing) {
        return;
    }
    if (const Ops& behaviour = ops::of(node.kind); behaviour.stop != nullptr) {
        behaviour.stop(ctx, node, state);
    }
}

void tickOrStop(Ctx& ctx, std::size_t index) {
    const Node& node = nodeAt(ctx, index);
    NodeState& state = ctx.state.nodes[index];
    if (isOneShot(node.kind)) {
        doStop(ctx, index);
        return;
    }
    if (node.kind == Kind::kDoNothing) {
        if (ctx.now > state.endTimestamp) {
            doStop(ctx, index);
        }
        return;
    }
    if (isGate(node.kind)) {
        bool any = false;
        for (std::uint8_t child = 0; child < node.childCount; ++child) {
            const std::size_t at = node.firstChild + state.order[child];
            if (ctx.state.nodes[at].running) {
                tickOrStop(ctx, at);
            }
        }
        for (std::uint8_t child = 0; child < node.childCount; ++child) {
            any = any || ctx.state.nodes[node.firstChild + child].running;
        }
        if (!any) {
            doStop(ctx, index);
        }
        return;
    }
    const Ops& behaviour = ops::of(node.kind);
    const bool timedOut = !behaviour.neverTimesOut && ctx.now > state.endTimestamp;
    if (!timedOut && behaviour.canStillUse != nullptr && behaviour.canStillUse(ctx, node, state)) {
        if (behaviour.tick != nullptr) {
            behaviour.tick(ctx, node, state);
        }
    } else {
        doStop(ctx, index);
    }
}

} // namespace

/// Rebuilds the brain from the table this villager needs now, keeping what a save would keep.
void rebuild(Ctx& ctx) {
    VillagerState& state = ctx.state;
    if (state.table != nullptr) {
        // parity: `Brain.stopAll` before the old brain is thrown away.
        for (const std::uint8_t index : state.table->top) {
            if (state.nodes[index].running) {
                doStop(ctx, index);
            }
        }
    }
    const bool baby = look::isBaby(ctx.self);
    state.table = &tableFor(baby, ctx.self.profession == "farmer");
    state.nodes.clear();
    state.nodes.resize(state.table->nodes.size());
    for (std::size_t index = 0; index < state.table->nodes.size(); ++index) {
        for (std::uint8_t child = 0; child < state.nodes[index].order.size(); ++child) {
            state.nodes[index].order[child] = child;
        }
    }
    // parity: `Brain.pack` keeps only the memories with a codec.
    Memories kept;
    kept.home = state.memories.home;
    kept.jobSite = state.memories.jobSite;
    kept.potentialJobSite = state.memories.potentialJobSite;
    kept.meetingPoint = state.memories.meetingPoint;
    kept.golemDetectedRecently = state.memories.golemDetectedRecently;
    kept.lastSlept = state.memories.lastSlept;
    kept.lastWoken = state.memories.lastWoken;
    kept.lastWorkedAtPoi = state.memories.lastWorkedAtPoi;
    state.memories = std::move(kept);
    state.active = bit(Activity::kCore) | bit(Activity::kIdle);
    state.defaultActivity = Activity::kIdle;
    state.lastScheduleUpdate = -9999;
    state.sensorsStarted = false;
    state.wasBaby = baby;
    state.refresh = false;
    // parity: `registerBrainGoals` asks the schedule straight away.
    brain::updateActivityFromSchedule(ctx);
}

void Memories::tick() noexcept {
    home.tick();
    jobSite.tick();
    potentialJobSite.tick();
    meetingPoint.tick();
    nearestBed.tick();
    hidingPlace.tick();
    secondaryJobSite.tick();
    doorsToClose.tick();
    lookTarget.tick();
    walkTarget.tick();
    path.tick();
    cantReachWalkTargetSince.tick();
    lastSlept.tick();
    lastWoken.tick();
    lastWorkedAtPoi.tick();
    heardBellTime.tick();
    golemDetectedRecently.tick();
    interactionTarget.tick();
    breedTarget.tick();
    hurtByEntity.tick();
    nearestHostile.tick();
    nearestVisiblePlayer.tick();
    nearestVisibleWantedItem.tick();
    hurtBy.tick();
    nearestLivingEntities.tick();
    visibleLivingEntities.tick();
    nearestPlayers.tick();
    visibleVillagerBabies.tick();
}

bool Memories::visible(std::int32_t entityId) const {
    return visibleLivingEntities.present() &&
           std::ranges::find(*visibleLivingEntities.value, entityId) != visibleLivingEntities.value->end();
}

Memory<BlockPos>* Memories::position(MemoryId id) {
    switch (id) {
    case MemoryId::kHome:
        return &home;
    case MemoryId::kJobSite:
        return &jobSite;
    case MemoryId::kPotentialJobSite:
        return &potentialJobSite;
    case MemoryId::kMeetingPoint:
        return &meetingPoint;
    default:
        return nullptr;
    }
}

Memory<std::int32_t>* Memories::entity(MemoryId id) {
    switch (id) {
    case MemoryId::kNearestHostile:
        return &nearestHostile;
    case MemoryId::kHurtByEntity:
        return &hurtByEntity;
    case MemoryId::kInteractionTarget:
        return &interactionTarget;
    case MemoryId::kBreedTarget:
        return &breedTarget;
    default:
        return nullptr;
    }
}

bool Memories::present(MemoryId id) const {
    auto& self = const_cast<Memories&>(*this);
    if (const Memory<BlockPos>* found = self.position(id); found != nullptr) {
        return found->present();
    }
    if (const Memory<std::int32_t>* found = self.entity(id); found != nullptr) {
        return found->present();
    }
    if (id == MemoryId::kSecondaryJobSite) {
        return secondaryJobSite.present();
    }
    if (id == MemoryId::kVisibleVillagerBabies) {
        return visibleVillagerBabies.present();
    }
    return false;
}

Tracker Tracker::ofSpot(const std::array<double, 3>& spot) {
    return Tracker{.spot = spot,
                   .block = BlockPos{static_cast<std::int32_t>(std::floor(spot[0])),
                                     static_cast<std::int32_t>(std::floor(spot[1])),
                                     static_cast<std::int32_t>(std::floor(spot[2]))}};
}

namespace brain {

bool isActive(const VillagerState& state, Activity activity) { return (state.active & bit(activity)) != 0; }

namespace {

/// parity: the activity requirements `Villager.BRAIN_PROVIDER` gives work and meet.
[[nodiscard]] bool requirementsMet(const Ctx& ctx, Activity activity) {
    if (activity == Activity::kWork) {
        return ctx.mem.jobSite.present();
    }
    if (activity == Activity::kMeet) {
        return ctx.mem.meetingPoint.present();
    }
    return true;
}

/// parity: `Brain.setActiveActivity`. No villager activity erases memories when it stops.
void setActive(Ctx& ctx, Activity activity) {
    if (!isActive(ctx.state, activity)) {
        ctx.state.active = static_cast<std::uint16_t>(bit(Activity::kCore) | bit(activity));
    }
}

[[nodiscard]] Activity scheduled(const Ctx& ctx) {
    using ai::villagerSchedule::Activity;
    switch (ai::villagerSchedule::activityAt(ctx.mob.dayTime, look::isBaby(ctx.self))) {
    case Activity::Work:
        return villager::Activity::kWork;
    case Activity::Meet:
        return villager::Activity::kMeet;
    case Activity::Rest:
        return villager::Activity::kRest;
    case Activity::Play:
        return villager::Activity::kPlay;
    case Activity::Idle:
        break;
    }
    return villager::Activity::kIdle;
}

} // namespace

void setActiveActivityIfPossible(Ctx& ctx, Activity activity) {
    if (requirementsMet(ctx, activity)) {
        setActive(ctx, activity);
    } else {
        setActive(ctx, ctx.state.defaultActivity);
    }
}

void updateActivityFromSchedule(Ctx& ctx) {
    if (ctx.now - ctx.state.lastScheduleUpdate <= 20) {
        return;
    }
    ctx.state.lastScheduleUpdate = ctx.now;
    const Activity wanted = scheduled(ctx);
    if (!isActive(ctx.state, wanted)) {
        setActiveActivityIfPossible(ctx, wanted);
    }
}

std::optional<Activity> activeNonCore(const VillagerState& state) {
    for (std::uint8_t index = 1; index < static_cast<std::uint8_t>(Activity::kCount); ++index) {
        if (isActive(state, static_cast<Activity>(index))) {
            return static_cast<Activity>(index);
        }
    }
    return std::nullopt;
}

void refresh(Ctx& ctx) { rebuild(ctx); }

} // namespace brain

namespace engine {

void run(Ctx& ctx) {
    VillagerState& state = ctx.state;
    // parity: `Brain.tick`: forget, sense, start what may start, tick what runs.
    state.memories.tick();
    sensors::tick(ctx);
    for (const std::uint8_t index : state.table->top) {
        const Node& node = state.table->nodes[index];
        if (brain::isActive(state, node.activity) && !state.nodes[index].running) {
            static_cast<void>(tryStart(ctx, index));
        }
    }
    for (const std::uint8_t index : state.table->top) {
        if (state.nodes[index].running) {
            tickOrStop(ctx, index);
        }
    }
}

void rebuild(Ctx& ctx) { villager::rebuild(ctx); }

} // namespace engine

} // namespace basalt::server::runtime::entity::villager
