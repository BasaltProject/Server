#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::runtime::entity::ai {
class MobContext;
struct PlayerView;
} // namespace basalt::server::runtime::entity::ai

namespace basalt::server::runtime::entity::villager {

/// Every behaviour a villager's brain is built from. parity: the classes `VillagerGoalPackages`
/// instantiates, in the order the packages list them.
enum class Kind : std::uint8_t {
    kSwim,
    kInteractWithDoor,
    kLookAtTargetSink,
    kPanicTrigger,
    kWakeUp,
    kReactToBell,
    kSetRaidStatus,
    kValidateNearbyPoi,
    kMoveToTargetSink,
    kPoiCompetitorScan,
    kFollowTradingPlayer,
    kGoToWantedItem,
    kAcquirePoi,
    kGoToPotentialJobSite,
    kYieldJobSite,
    kAssignProfession,
    kResetProfession,
    kWorkAtPoi,
    kWorkAtComposter,
    kStrollAroundPoi,
    kStrollToPoi,
    kStrollToPoiList,
    kHarvestFarmland,
    kUseBonemeal,
    kShowTrades,
    kSetLookAndInteract,
    kWalkToBlockMemory,
    kGiveGiftToHero,
    kUpdateSchedule,
    kInteractWith,
    kVillageBoundStroll,
    kWalkToLookTarget,
    kJumpOnBed,
    kDoNothing,
    kPlayTag,
    kSetEntityLookTarget,
    kTradeWithVillager,
    kMakeLove,
    kSleepInBed,
    kClosestHomeAsWalkTarget,
    kInsideBrownianWalk,
    kGoToClosestVillage,
    kSocializeAtBell,
    kCalmDown,
    kWalkAwayFrom,
    kRingBell,
    kResetRaidStatus,
    kMoveToSkySeeingSpot,
    kCelebrate,
    kLocateHidingPlace,
    kSetHiddenState,
    kRunOne,
    kGate,
    kTriggerGate,
    kIfRaidWon,
    kIfRaidOngoing,
    kCount,
};

/// Who a look or interaction behaviour picks from the ones it can see.
enum class Filter : std::uint8_t {
    kAny,
    kVillager,
    kPlayer,
    kCat,
    kCreature,
    kWaterCreature,
    kAxolotls,
    kUndergroundWaterCreature,
    kWaterAmbient,
    kMonster,
};

/// One behaviour in a brain, and how it was configured. Children follow their parent's
/// `firstChild`; top-level nodes carry the priority and activity they were registered under.
struct Node {
    Kind kind = Kind::kDoNothing;
    std::uint8_t priority = 0;
    Activity activity = Activity::kCore;
    float speed = 0.0F;
    std::int32_t a = 0;
    std::int32_t b = 0;
    std::int32_t c = 0;
    MemoryId memory = MemoryId::kNone;
    MemoryId memory2 = MemoryId::kNone;
    world::poi::PoiMask poi = 0;
    std::int8_t event = -1;
    bool flag = false;
    Filter filter = Filter::kAny;
    std::int32_t minDuration = 60;
    std::int32_t maxDuration = 60;
    std::uint8_t firstChild = 0;
    std::uint8_t childCount = 0;
    std::int32_t weight = 1;
    bool shuffled = true;
    MemoryId eraseOnStop = MemoryId::kNone;
    MemoryId requireAbsent = MemoryId::kNone;
};

/// A whole brain: its nodes, and the top-level ones in the order the brain walks them.
struct BrainTable {
    std::vector<Node> nodes;
    std::vector<std::uint8_t> top;
};

/// parity: `Villager.BRAIN_PROVIDER` for a baby, a farmer, or anybody else.
[[nodiscard]] const BrainTable& tableFor(bool baby, bool farmer);

/// What every behaviour is handed.
struct Ctx {
    ai::MobContext& mob;
    ServerEntity& self;
    VillagerState& state;
    Memories& mem;
    world::poi::PoiManager& poi;
    VillagerWorld& world;
    random::RandomSource& random;
    std::int64_t now;
};

/// How one kind of behaviour runs. parity: `Behavior`'s hooks, or a one-shot's trigger.
struct Ops {
    bool (*check)(Ctx&, const Node&, NodeState&) = nullptr;
    void (*start)(Ctx&, const Node&, NodeState&) = nullptr;
    bool (*canStillUse)(Ctx&, const Node&, NodeState&) = nullptr;
    void (*tick)(Ctx&, const Node&, NodeState&) = nullptr;
    void (*stop)(Ctx&, const Node&, NodeState&) = nullptr;
    bool neverTimesOut = false;
    bool (*trigger)(Ctx&, const Node&, NodeState&) = nullptr;
};

/// What the batch caches and lists of the fussier behaviours keep between turns.
struct NodeExtra {
    struct Retry {
        std::int64_t previous = 0;
        std::int64_t next = 0;
        std::int32_t delay = 0;
    };

    std::vector<std::pair<std::int64_t, Retry>> retries;
    std::vector<std::pair<std::int64_t, std::int64_t>> batch;
    std::vector<BlockPos> positions;
    std::vector<server::world::loot::ItemStack> display;
    std::optional<std::string> shownFor;
};

[[nodiscard]] NodeExtra& extraOf(NodeState& state);

namespace ops {

[[nodiscard]] const Ops& of(Kind kind);

// Registered by the file that implements each group.
[[nodiscard]] const Ops* core(Kind kind);
[[nodiscard]] const Ops* poi(Kind kind);
[[nodiscard]] const Ops* work(Kind kind);
[[nodiscard]] const Ops* social(Kind kind);
[[nodiscard]] const Ops* raid(Kind kind);

} // namespace ops

/// Rebuilds a villager's brain. parity: `Villager.refreshBrain` and the brain a new villager gets.
void rebuild(Ctx& ctx);

namespace engine {

/// parity: `Brain.tick`.
void run(Ctx& ctx);

void rebuild(Ctx& ctx);

} // namespace engine

namespace sensors {

/// parity: every sensor `Villager.BRAIN_PROVIDER` lists, each on its own clock.
void tick(Ctx& ctx);

} // namespace sensors

namespace brain {

[[nodiscard]] bool isActive(const VillagerState& state, Activity activity);

/// parity: `Brain.setActiveActivityIfPossible`.
void setActiveActivityIfPossible(Ctx& ctx, Activity activity);

/// parity: `Brain.updateActivityFromSchedule`.
void updateActivityFromSchedule(Ctx& ctx);

/// parity: `Brain.getActiveNonCoreActivity`.
[[nodiscard]] std::optional<Activity> activeNonCore(const VillagerState& state);

/// parity: `Villager.refreshBrain`, which stops what runs and keeps only what is saved.
void refresh(Ctx& ctx);

/// parity: `Villager.releasePoi` for one of the four remembered spots.
void releasePoi(Ctx& ctx, MemoryId id);

/// parity: `Villager.spawnGolemIfNeeded`.
void spawnGolemIfNeeded(Ctx& ctx, std::int32_t villagersNeeded);

/// parity: `Villager.gossip`.
void gossip(Ctx& ctx, ServerEntity& other);

} // namespace brain

namespace trading {

/// parity: `Villager.shouldRestock`, which also starts a new day's count.
[[nodiscard]] bool shouldRestock(Ctx& ctx);

/// parity: `Villager.restock`.
void restock(Ctx& ctx);

} // namespace trading

namespace look {

/// Where an entity stands, whether it is a mob or a player.
[[nodiscard]] std::optional<std::array<double, 3>> feet(Ctx& ctx, std::int32_t entityId);

[[nodiscard]] ServerEntity* mob(Ctx& ctx, std::int32_t entityId);

[[nodiscard]] const ai::PlayerView* player(Ctx& ctx, std::int32_t entityId);

[[nodiscard]] bool alive(Ctx& ctx, std::int32_t entityId);

[[nodiscard]] double distanceSqr(Ctx& ctx, std::int32_t entityId);

/// parity: `PositionTracker.currentPosition`.
[[nodiscard]] std::optional<std::array<double, 3>> position(Ctx& ctx, const Tracker& tracker);

/// parity: `PositionTracker.currentBlockPosition`.
[[nodiscard]] std::optional<BlockPos> block(Ctx& ctx, const Tracker& tracker);

/// parity: `BehaviorUtils.setWalkAndLookTargetMemories`.
void walkAndLook(Ctx& ctx, const Tracker& target, float speed, std::int32_t closeEnough);

/// parity: `BehaviorUtils.lockGazeAndWalkToEachOther`, whose other half is another villager.
void lockGaze(Ctx& ctx, ServerEntity& other, float speed, std::int32_t closeEnough);

/// Whether an entity is a villager with a brain, and that brain.
[[nodiscard]] VillagerState* villagerState(Ctx& ctx, std::int32_t entityId);

[[nodiscard]] bool matches(Ctx& ctx, std::int32_t entityId, Filter filter);

/// parity: `NearestVisibleLivingEntities.findClosest`.
[[nodiscard]] std::optional<std::int32_t> closestVisible(Ctx& ctx, Filter filter, double maxDistanceSqr);

[[nodiscard]] BlockPos blockPosition(const ServerEntity& mob);

/// Where a path for this mob starts. parity: `WalkNodeEvaluator.getStart`.
[[nodiscard]] BlockPos pathStart(Ctx& ctx, const ServerEntity& mob);

[[nodiscard]] bool closerToCenterThan(const BlockPos& pos, const ServerEntity& mob, double distance);

[[nodiscard]] std::int32_t manhattan(const BlockPos& one, const BlockPos& other);

[[nodiscard]] bool isBaby(const ServerEntity& mob);

} // namespace look

namespace villagerData {

[[nodiscard]] bool hasJob(const ServerEntity& villager);

/// parity: `VillagerData.withProfession` followed by `refreshBrain`.
void setProfession(Ctx& ctx, std::string_view profession);

/// parity: `Villager.canBreed`.
[[nodiscard]] bool canBreed(const ServerEntity& villager);

/// parity: `countFoodPointsInInventory`.
[[nodiscard]] std::int32_t foodPoints(const VillagerState& state);

/// parity: `eatAndDigestFood`.
void eatAndDigest(VillagerState& state);

[[nodiscard]] std::int32_t countItem(const VillagerState& state, std::string_view item);

/// Takes up to `count` of an item out of the inventory, from the first slot on.
void removeItem(VillagerState& state, std::string_view item, std::int32_t count);

/// parity: `SimpleContainer.addItem`. Answers what did not fit.
[[nodiscard]] std::int32_t addItem(VillagerState& state, const server::world::loot::ItemStack& stack);

/// parity: `Villager.wantsToPickUp`.
[[nodiscard]] bool wantsToPickUp(const ServerEntity& villager, const VillagerState& state,
                                 const server::world::loot::ItemStack& stack);

} // namespace villagerData

} // namespace basalt::server::runtime::entity::villager
