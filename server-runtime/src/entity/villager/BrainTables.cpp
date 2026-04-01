#include "BrainInternals.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>

namespace basalt::server::runtime::entity::villager {
namespace {

using world::poi::maskOf;
using world::poi::PoiType;

/// parity: `Villager.SPEED_MODIFIER`, which every package is built with.
constexpr float kSpeed = 0.5F;
/// parity: `VillagerGoalPackages.STROLL_SPEED_MODIFIER`.
constexpr float kStroll = 0.4F;

/// Which job-site predicate an `AcquirePoi` or `ValidateNearbyPoi` reads, worked out from the
/// profession at the time rather than baked into the table.
enum PoiSource : std::int32_t { kFixed = 0, kHeld = 1, kAcquirable = 2 };

struct Child {
    Node node;
    std::int32_t weight;
};

class Builder {
public:
    /// A top-level behaviour. Answers its index.
    std::uint8_t add(Activity activity, std::uint8_t priority, Node node) {
        node.activity = activity;
        node.priority = priority;
        const auto index = static_cast<std::uint8_t>(table_.nodes.size());
        table_.nodes.push_back(node);
        table_.top.push_back(index);
        return index;
    }

    /// A composite whose children follow it.
    void addGate(Activity activity, std::uint8_t priority, Node gate, std::initializer_list<Child> children) {
        const std::uint8_t index = add(activity, priority, gate);
        fill(index, children);
    }

    /// parity: `BehaviorBuilder.sequence(triggerIf(...), gate)`, a condition around one composite.
    void addGuardedGate(Activity activity, std::uint8_t priority, Kind guard, Node gate,
                        std::initializer_list<Child> children) {
        const std::uint8_t outer = add(activity, priority, Node{.kind = guard});
        table_.nodes[outer].firstChild = static_cast<std::uint8_t>(table_.nodes.size());
        table_.nodes[outer].childCount = 1;
        const auto inner = static_cast<std::uint8_t>(table_.nodes.size());
        table_.nodes.push_back(gate);
        fill(inner, children);
    }

    void addGuarded(Activity activity, std::uint8_t priority, Kind guard, Node child) {
        const std::uint8_t outer = add(activity, priority, Node{.kind = guard});
        table_.nodes[outer].firstChild = static_cast<std::uint8_t>(table_.nodes.size());
        table_.nodes[outer].childCount = 1;
        table_.nodes.push_back(child);
    }

    [[nodiscard]] BrainTable finish() {
        // parity: `availableBehaviorsByPriority`, a tree map of priorities, each walking its
        // activities and each of those its behaviours in the order they were added.
        std::ranges::stable_sort(table_.top, [this](std::uint8_t one, std::uint8_t other) {
            const Node& first = table_.nodes[one];
            const Node& second = table_.nodes[other];
            if (first.priority != second.priority) {
                return first.priority < second.priority;
            }
            return static_cast<std::uint8_t>(first.activity) < static_cast<std::uint8_t>(second.activity);
        });
        return std::move(table_);
    }

private:
    void fill(std::uint8_t parent, std::initializer_list<Child> children) {
        table_.nodes[parent].firstChild = static_cast<std::uint8_t>(table_.nodes.size());
        table_.nodes[parent].childCount = static_cast<std::uint8_t>(children.size());
        for (const Child& child : children) {
            Node node = child.node;
            node.weight = child.weight;
            table_.nodes.push_back(node);
        }
    }

    BrainTable table_;
};

[[nodiscard]] Node lookAt(Filter filter) { return Node{.kind = Kind::kSetEntityLookTarget, .a = 8, .filter = filter}; }

[[nodiscard]] Node doNothing(std::int32_t min, std::int32_t max) {
    return Node{.kind = Kind::kDoNothing, .minDuration = min, .maxDuration = max};
}

[[nodiscard]] Node walkToBlockMemory(MemoryId memory, float speed, std::int32_t closeEnough, std::int32_t tooFar,
                                     std::int32_t tooLong) {
    return Node{.kind = Kind::kWalkToBlockMemory,
                .speed = speed,
                .a = closeEnough,
                .b = tooFar,
                .c = tooLong,
                .memory = memory};
}

[[nodiscard]] Node villageStroll(float speed, std::int32_t horizontal = 10, std::int32_t vertical = 7) {
    return Node{.kind = Kind::kVillageBoundStroll, .speed = speed, .a = horizontal, .b = vertical};
}

[[nodiscard]] Node interactWith(Filter filter, MemoryId memory, bool breeding = false) {
    return Node{.kind = Kind::kInteractWith,
                .speed = kSpeed,
                .a = 8,
                .b = 2,
                .memory = memory,
                .flag = breeding,
                .filter = filter};
}

/// parity: `getMinimalLookBehavior`.
void minimalLook(Builder& builder, Activity activity) {
    builder.addGate(activity, 5, Node{.kind = Kind::kRunOne},
                    {{lookAt(Filter::kVillager), 2}, {lookAt(Filter::kPlayer), 2}, {doNothing(30, 60), 8}});
}

/// parity: `getFullLookBehavior`.
void fullLook(Builder& builder, Activity activity) {
    builder.addGate(activity, 5, Node{.kind = Kind::kRunOne},
                    {{lookAt(Filter::kCat), 8},
                     {lookAt(Filter::kVillager), 2},
                     {lookAt(Filter::kPlayer), 2},
                     {lookAt(Filter::kCreature), 1},
                     {lookAt(Filter::kWaterCreature), 1},
                     {lookAt(Filter::kAxolotls), 1},
                     {lookAt(Filter::kUndergroundWaterCreature), 1},
                     {lookAt(Filter::kWaterAmbient), 1},
                     {lookAt(Filter::kMonster), 1},
                     {doNothing(30, 60), 2}});
}

/// parity: `getCorePackage`.
void core(Builder& builder) {
    constexpr Activity kCore = Activity::kCore;
    builder.add(kCore, 0, Node{.kind = Kind::kSwim, .speed = 0.8F});
    builder.add(kCore, 0, Node{.kind = Kind::kInteractWithDoor});
    builder.add(kCore, 0, Node{.kind = Kind::kLookAtTargetSink, .minDuration = 45, .maxDuration = 90});
    builder.add(kCore, 0, Node{.kind = Kind::kPanicTrigger});
    builder.add(kCore, 0, Node{.kind = Kind::kWakeUp});
    builder.add(kCore, 0, Node{.kind = Kind::kReactToBell});
    builder.add(kCore, 0, Node{.kind = Kind::kSetRaidStatus});
    builder.add(kCore, 0, Node{.kind = Kind::kValidateNearbyPoi, .a = kHeld, .memory = MemoryId::kJobSite});
    builder.add(kCore, 0,
                Node{.kind = Kind::kValidateNearbyPoi, .a = kAcquirable, .memory = MemoryId::kPotentialJobSite});
    builder.add(kCore, 1, Node{.kind = Kind::kMoveToTargetSink, .minDuration = 150, .maxDuration = 250});
    builder.add(kCore, 2, Node{.kind = Kind::kPoiCompetitorScan});
    builder.add(
        kCore, 3,
        Node{
            .kind = Kind::kFollowTradingPlayer, .speed = kSpeed, .minDuration = 0x7FFFFFFF, .maxDuration = 0x7FFFFFFF});
    builder.add(kCore, 5, Node{.kind = Kind::kGoToWantedItem, .speed = kSpeed, .a = 4});
    builder.add(kCore, 6,
                Node{.kind = Kind::kAcquirePoi,
                     .a = kAcquirable,
                     .memory = MemoryId::kPotentialJobSite,
                     .memory2 = MemoryId::kJobSite,
                     .flag = true});
    builder.add(kCore, 7,
                Node{.kind = Kind::kGoToPotentialJobSite, .speed = kSpeed, .minDuration = 1200, .maxDuration = 1200});
    builder.add(kCore, 8, Node{.kind = Kind::kYieldJobSite, .speed = kSpeed});
    builder.add(kCore, 10,
                Node{.kind = Kind::kAcquirePoi,
                     .a = kFixed,
                     .b = 1,
                     .memory = MemoryId::kHome,
                     .memory2 = MemoryId::kHome,
                     .poi = maskOf(PoiType::kHome),
                     .event = 14,
                     .flag = false});
    builder.add(kCore, 10,
                Node{.kind = Kind::kAcquirePoi,
                     .a = kFixed,
                     .memory = MemoryId::kMeetingPoint,
                     .memory2 = MemoryId::kMeetingPoint,
                     .poi = maskOf(PoiType::kMeeting),
                     .event = 14,
                     .flag = true});
    builder.add(kCore, 10, Node{.kind = Kind::kAssignProfession});
    builder.add(kCore, 10, Node{.kind = Kind::kResetProfession});
}

/// parity: `getWorkPackage`.
void work(Builder& builder, bool farmer) {
    constexpr Activity kWork = Activity::kWork;
    minimalLook(builder, kWork);
    builder.addGate(
        kWork, 5, Node{.kind = Kind::kRunOne},
        {{Node{.kind = farmer ? Kind::kWorkAtComposter : Kind::kWorkAtPoi}, 7},
         {Node{.kind = Kind::kStrollAroundPoi, .speed = kStroll, .a = 4, .memory = MemoryId::kJobSite}, 2},
         {Node{.kind = Kind::kStrollToPoi, .speed = kStroll, .a = 1, .b = 10, .memory = MemoryId::kJobSite}, 5},
         {Node{.kind = Kind::kStrollToPoiList, .speed = kSpeed, .a = 1, .b = 6, .memory = MemoryId::kJobSite}, 5},
         {Node{.kind = Kind::kHarvestFarmland}, farmer ? 2 : 5},
         {Node{.kind = Kind::kUseBonemeal}, farmer ? 4 : 7}});
    builder.add(kWork, 10, Node{.kind = Kind::kShowTrades, .minDuration = 400, .maxDuration = 1600});
    builder.add(kWork, 10, Node{.kind = Kind::kSetLookAndInteract, .a = 4, .filter = Filter::kPlayer});
    builder.add(kWork, 2, walkToBlockMemory(MemoryId::kJobSite, kSpeed, 9, 100, 1200));
    builder.add(kWork, 3, Node{.kind = Kind::kGiveGiftToHero, .minDuration = 100, .maxDuration = 100});
    builder.add(kWork, 99, Node{.kind = Kind::kUpdateSchedule});
}

/// parity: `getPlayPackage`.
void play(Builder& builder) {
    constexpr Activity kPlay = Activity::kPlay;
    builder.add(kPlay, 0, Node{.kind = Kind::kMoveToTargetSink, .minDuration = 80, .maxDuration = 120});
    fullLook(builder, kPlay);
    builder.add(kPlay, 5, Node{.kind = Kind::kPlayTag});
    builder.addGate(kPlay, 5, Node{.kind = Kind::kRunOne, .requireAbsent = MemoryId::kVisibleVillagerBabies},
                    {{interactWith(Filter::kVillager, MemoryId::kInteractionTarget), 2},
                     {interactWith(Filter::kCat, MemoryId::kInteractionTarget), 1},
                     {villageStroll(kSpeed), 1},
                     {Node{.kind = Kind::kWalkToLookTarget, .speed = kSpeed, .a = 2}, 1},
                     {Node{.kind = Kind::kJumpOnBed, .speed = kSpeed}, 2},
                     {doNothing(20, 40), 2}});
    builder.add(kPlay, 99, Node{.kind = Kind::kUpdateSchedule});
}

/// parity: `getRestPackage`.
void rest(Builder& builder) {
    constexpr Activity kRest = Activity::kRest;
    builder.add(kRest, 2, walkToBlockMemory(MemoryId::kHome, kSpeed, 1, 150, 1200));
    builder.add(
        kRest, 3,
        Node{.kind = Kind::kValidateNearbyPoi, .a = kFixed, .memory = MemoryId::kHome, .poi = maskOf(PoiType::kHome)});
    builder.add(kRest, 3, Node{.kind = Kind::kSleepInBed});
    builder.addGate(kRest, 5, Node{.kind = Kind::kRunOne, .requireAbsent = MemoryId::kHome},
                    {{Node{.kind = Kind::kClosestHomeAsWalkTarget, .speed = kSpeed}, 1},
                     {Node{.kind = Kind::kInsideBrownianWalk, .speed = kSpeed}, 4},
                     {Node{.kind = Kind::kGoToClosestVillage, .speed = kSpeed, .a = 4}, 2},
                     {doNothing(20, 40), 2}});
    minimalLook(builder, kRest);
    builder.add(kRest, 99, Node{.kind = Kind::kUpdateSchedule});
}

/// parity: `getMeetPackage`.
void meet(Builder& builder) {
    constexpr Activity kMeet = Activity::kMeet;
    builder.addGate(
        kMeet, 2, Node{.kind = Kind::kTriggerGate},
        {{Node{.kind = Kind::kStrollAroundPoi, .speed = kStroll, .a = 40, .memory = MemoryId::kMeetingPoint}, 2},
         {Node{.kind = Kind::kSocializeAtBell}, 2}});
    builder.add(kMeet, 10, Node{.kind = Kind::kShowTrades, .minDuration = 400, .maxDuration = 1600});
    builder.add(kMeet, 10, Node{.kind = Kind::kSetLookAndInteract, .a = 4, .filter = Filter::kPlayer});
    builder.add(kMeet, 2, walkToBlockMemory(MemoryId::kMeetingPoint, kSpeed, 6, 100, 200));
    builder.add(kMeet, 3, Node{.kind = Kind::kGiveGiftToHero, .minDuration = 100, .maxDuration = 100});
    builder.add(kMeet, 3,
                Node{.kind = Kind::kValidateNearbyPoi,
                     .a = kFixed,
                     .memory = MemoryId::kMeetingPoint,
                     .poi = maskOf(PoiType::kMeeting)});
    builder.addGate(kMeet, 3, Node{.kind = Kind::kGate, .shuffled = false, .eraseOnStop = MemoryId::kInteractionTarget},
                    {{Node{.kind = Kind::kTradeWithVillager}, 1}});
    fullLook(builder, kMeet);
    builder.add(kMeet, 99, Node{.kind = Kind::kUpdateSchedule});
}

/// parity: `getIdlePackage`.
void idle(Builder& builder) {
    constexpr Activity kIdle = Activity::kIdle;
    builder.addGate(kIdle, 2, Node{.kind = Kind::kRunOne},
                    {{interactWith(Filter::kVillager, MemoryId::kInteractionTarget), 2},
                     {interactWith(Filter::kVillager, MemoryId::kBreedTarget, true), 1},
                     {interactWith(Filter::kCat, MemoryId::kInteractionTarget), 1},
                     {villageStroll(kSpeed), 1},
                     {Node{.kind = Kind::kWalkToLookTarget, .speed = kSpeed, .a = 2}, 1},
                     {Node{.kind = Kind::kJumpOnBed, .speed = kSpeed}, 1},
                     {doNothing(30, 60), 1}});
    builder.add(kIdle, 3, Node{.kind = Kind::kGiveGiftToHero, .minDuration = 100, .maxDuration = 100});
    builder.add(kIdle, 3, Node{.kind = Kind::kSetLookAndInteract, .a = 4, .filter = Filter::kPlayer});
    builder.add(kIdle, 3, Node{.kind = Kind::kShowTrades, .minDuration = 400, .maxDuration = 1600});
    builder.addGate(kIdle, 3, Node{.kind = Kind::kGate, .shuffled = false, .eraseOnStop = MemoryId::kInteractionTarget},
                    {{Node{.kind = Kind::kTradeWithVillager}, 1}});
    builder.addGate(kIdle, 3, Node{.kind = Kind::kGate, .shuffled = false, .eraseOnStop = MemoryId::kBreedTarget},
                    {{Node{.kind = Kind::kMakeLove, .minDuration = 350, .maxDuration = 350}, 1}});
    fullLook(builder, kIdle);
    builder.add(kIdle, 99, Node{.kind = Kind::kUpdateSchedule});
}

/// parity: `getPanicPackage`, `getPreRaidPackage`, `getRaidPackage` and `getHidePackage`.
void danger(Builder& builder) {
    constexpr float kRunaway = kSpeed * 1.5F;
    builder.add(Activity::kPanic, 0, Node{.kind = Kind::kCalmDown});
    builder.add(Activity::kPanic, 1,
                Node{.kind = Kind::kWalkAwayFrom, .speed = kRunaway, .a = 6, .memory = MemoryId::kNearestHostile});
    builder.add(Activity::kPanic, 1,
                Node{.kind = Kind::kWalkAwayFrom, .speed = kRunaway, .a = 6, .memory = MemoryId::kHurtByEntity});
    builder.add(Activity::kPanic, 3, villageStroll(kRunaway, 2, 2));
    minimalLook(builder, Activity::kPanic);

    builder.add(Activity::kPreRaid, 0, Node{.kind = Kind::kRingBell});
    builder.addGate(Activity::kPreRaid, 0, Node{.kind = Kind::kTriggerGate},
                    {{walkToBlockMemory(MemoryId::kMeetingPoint, kSpeed * 1.5F, 2, 150, 200), 6},
                     {villageStroll(kSpeed * 1.5F), 2}});
    minimalLook(builder, Activity::kPreRaid);
    builder.add(Activity::kPreRaid, 99, Node{.kind = Kind::kResetRaidStatus});

    builder.addGuardedGate(
        Activity::kRaid, 0, Kind::kIfRaidWon, Node{.kind = Kind::kTriggerGate},
        {{Node{.kind = Kind::kMoveToSkySeeingSpot, .speed = kSpeed}, 5}, {villageStroll(kSpeed * 1.1F), 2}});
    builder.add(Activity::kRaid, 0, Node{.kind = Kind::kCelebrate, .minDuration = 600, .maxDuration = 600});
    builder.addGuarded(Activity::kRaid, 2, Kind::kIfRaidOngoing,
                       Node{.kind = Kind::kLocateHidingPlace, .speed = kSpeed * 1.4F, .a = 24, .b = 1});
    minimalLook(builder, Activity::kRaid);
    builder.add(Activity::kRaid, 99, Node{.kind = Kind::kResetRaidStatus});

    builder.add(Activity::kHide, 0, Node{.kind = Kind::kSetHiddenState, .a = 15, .b = 3});
    builder.add(Activity::kHide, 1, Node{.kind = Kind::kLocateHidingPlace, .speed = kSpeed * 1.25F, .a = 32, .b = 2});
    minimalLook(builder, Activity::kHide);
}

[[nodiscard]] BrainTable build(bool baby, bool farmer) {
    Builder builder;
    // parity: the activities in the order the provider adds them: work or play, then the core,
    // meet, rest, idle, panic, pre-raid, raid and hide.
    if (baby) {
        play(builder);
    } else {
        work(builder, farmer);
    }
    core(builder);
    meet(builder);
    rest(builder);
    idle(builder);
    danger(builder);
    return builder.finish();
}

} // namespace

const BrainTable& tableFor(bool baby, bool farmer) {
    static const BrainTable kBaby = build(true, false);
    static const BrainTable kFarmer = build(false, true);
    static const BrainTable kAdult = build(false, false);
    if (baby) {
        return kBaby;
    }
    return farmer ? kFarmer : kAdult;
}

} // namespace basalt::server::runtime::entity::villager
