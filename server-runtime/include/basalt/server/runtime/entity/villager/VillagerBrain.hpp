#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/villager/Gossips.hpp>
#include <basalt/server/runtime/entity/villager/VillagerMemories.hpp>
#include <basalt/server/runtime/world/Trades.hpp>
#include <basalt/server/world/loot/LootContext.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::runtime::entity {
class ServerEntity;
}

namespace basalt::server::runtime::entity::ai {
class MobContext;
}

namespace basalt::server::runtime::entity::villager {

struct BrainTable;
struct NodeExtra;

/// What a raid near a villager looks like to it. parity: `ServerLevel.getRaidAt` and the questions
/// its behaviours ask of the answer.
struct RaidView {
    bool active = false;
    bool victory = false;
    bool loss = false;
    bool stopped = false;
    bool firstWaveSpawned = false;
    bool betweenWaves = false;
};

/// One behaviour's own state, per villager. parity: the fields of each `Behavior` subclass and the
/// mutable captures of each `BehaviorBuilder` lambda.
struct NodeState {
    bool running = false;
    std::int64_t endTimestamp = 0;
    std::int64_t t0 = 0;
    std::int64_t t1 = 0;
    std::int32_t i0 = 0;
    std::int32_t i1 = 0;
    std::int32_t i2 = 0;
    std::optional<BlockPos> pos;
    /// A gate's children in the order its last shuffle left them.
    std::array<std::uint8_t, 12> order{};
    std::unique_ptr<NodeExtra> extra;
};

/// Everything a villager keeps beyond what every entity keeps.
///
/// parity: `Villager`'s fields and its `Brain`. Held by the entity like the warden's state.
struct VillagerState {
    VillagerState();
    VillagerState(const VillagerState&) = delete;
    VillagerState& operator=(const VillagerState&) = delete;
    VillagerState(VillagerState&&) noexcept;
    VillagerState& operator=(VillagerState&&) noexcept;
    ~VillagerState();

    Memories memories;

    /// parity: `Brain.activeActivities`, one bit per `Activity`.
    std::uint16_t active = 0;
    Activity defaultActivity = Activity::kIdle;
    std::int64_t lastScheduleUpdate = -9999;
    const BrainTable* table = nullptr;
    std::vector<NodeState> nodes;
    /// parity: each sensor's `timeToTick`.
    std::array<std::int64_t, 9> sensorClock{};
    bool sensorsStarted = false;

    /// parity: `NearestBedSensor`'s batch cache and counters.
    std::vector<std::pair<std::int64_t, std::int64_t>> bedCache;
    std::int32_t bedTried = 0;
    std::int64_t bedLastUpdate = 0;

    std::int32_t foodLevel = 0;
    Gossips gossips;
    std::int64_t lastGossipTime = 0;
    std::int64_t lastGossipDecay = 0;
    std::int64_t lastRestock = 0;
    std::int32_t restocksToday = 0;
    std::int64_t lastRestockCheckDay = 0;
    /// parity: `AbstractVillager.inventory`, eight slots.
    std::array<std::optional<server::world::loot::ItemStack>, 8> inventory;
    /// parity: what `ShowTradesToPlayer` or `UseBonemeal` put in its hand.
    std::optional<server::world::loot::ItemStack> mainHand;
    /// parity: `AbstractVillager.tradingPlayer`, by entity id.
    std::optional<std::int32_t> tradingPlayer;
    /// parity: `Villager.lastTradedPlayer`.
    std::optional<std::int32_t> lastTradedPlayer;
    std::int32_t unhappyCounter = 0;
    /// parity: `DATA_VILLAGER_DATA_FINALIZED`.
    bool dataFinalized = false;
    /// Set whenever the profession changes, so the brain is rebuilt before its next turn.
    bool refresh = true;
    bool wasBaby = false;
    /// The panic countdown last turn, so a fresh hit is told apart from an old one.
    std::int32_t seenPanic = 0;
};

/// What a villager's behaviours make the world do. The runtime supplies the one implementation.
class VillagerWorld {
public:
    VillagerWorld() = default;
    VillagerWorld(const VillagerWorld&) = default;
    VillagerWorld(VillagerWorld&&) = default;
    VillagerWorld& operator=(const VillagerWorld&) = default;
    VillagerWorld& operator=(VillagerWorld&&) = default;
    virtual ~VillagerWorld() = default;

    /// parity: `ServerLevel.getRaidAt`.
    [[nodiscard]] virtual std::optional<RaidView> raidAt(const ServerEntity& villager) = 0;

    /// parity: `Level.broadcastEntityEvent`.
    virtual void entityEvent(ServerEntity& mob, std::int8_t event) = 0;

    /// parity: `Mob.makeSound` and `Entity.playSound`.
    virtual void sound(ServerEntity& mob, std::string_view sound, float volume, float pitch) = 0;

    /// parity: `Level.playSound` at a block, in the blocks category.
    virtual void blockSound(ServerEntity& mob, const BlockPos& pos, std::string_view sound) = 0;

    /// parity: `Level.levelEvent`.
    virtual void levelEvent(ServerEntity& mob, std::int32_t event, const BlockPos& pos, std::int32_t data) = 0;

    /// parity: `Level.setBlockAndUpdate`, seen by everybody.
    virtual bool setBlock(ServerEntity& mob, const BlockPos& pos, std::int32_t stateId) = 0;

    /// parity: `Level.destroyBlock` with its drops.
    virtual void destroyBlock(ServerEntity& mob, const BlockPos& pos) = 0;

    /// parity: `DoorBlock.setOpen`, both halves and the sound.
    virtual void setDoorOpen(ServerEntity& mob, const BlockPos& pos, bool open) = 0;

    /// parity: `BellBlock.attemptToRing`.
    virtual void ringBell(ServerEntity& mob, const BlockPos& pos) = 0;

    /// The villager's offers, rolled up to its level the first time they are asked for. parity:
    /// `AbstractVillager.getOffers`.
    virtual std::vector<world::trades::Offer>* offers(ServerEntity& mob) = 0;

    /// parity: `ComposterBlock.insertItem`. Answers the level it reached, or nothing when this is no
    /// composter.
    virtual std::optional<std::int32_t> composterAdd(ServerEntity& mob, const BlockPos& pos,
                                                     const wire::common::Identifier& item) = 0;

    /// parity: `ComposterBlock.extractProduce`, the bone meal popped out of a full composter.
    virtual void composterExtract(ServerEntity& mob, const BlockPos& pos) = 0;

    /// parity: `BoneMealItem.growCrop`. Answers whether it grew.
    virtual bool growCrop(ServerEntity& mob, const BlockPos& pos) = 0;

    /// parity: `BehaviorUtils.throwItem`.
    virtual void throwItem(ServerEntity& mob, const server::world::loot::ItemStack& stack, double towardsX,
                           double towardsY, double towardsZ) = 0;

    /// parity: `Entity.spawnAtLocation`.
    virtual void dropItem(ServerEntity& mob, const server::world::loot::ItemStack& stack, float offsetY) = 0;

    /// parity: `Villager.dropFromGiftLootTable`, thrown at whoever it is for.
    virtual void throwGift(ServerEntity& mob, std::string_view lootTable, std::int32_t targetId) = 0;

    /// parity: `LivingEntity.startSleeping` with a bed, `stopSleeping` without one.
    virtual void setSleeping(ServerEntity& mob, std::optional<BlockPos> bed) = 0;

    /// The villager data changed, so everybody watching has to see its new clothes.
    virtual void villagerDataChanged(ServerEntity& mob) = 0;

    /// What `mainHand` holds changed.
    virtual void mainHandChanged(ServerEntity& mob) = 0;

    /// parity: `DATA_UNHAPPY_COUNTER`, which the client counts down by itself.
    virtual void unhappyChanged(ServerEntity& mob, std::int32_t counter) = 0;

    /// parity: `LivingEntity.addEffect`.
    virtual void addEffect(ServerEntity& mob, std::string_view effect, std::int32_t duration,
                           std::int32_t amplifier) = 0;

    /// parity: `SpawnUtil.trySpawnMob` for an iron golem next to this villager.
    virtual bool summonGolem(ServerEntity& mob) = 0;

    /// parity: `Villager.getBreedOffspring` and the spawn that follows it.
    virtual ServerEntity* spawnChild(ServerEntity& mob, ServerEntity& partner) = 0;

    /// parity: `CelebrateVillagersSurvivedRaid`'s rocket, in one of the dye colours.
    virtual void launchFirework(ServerEntity& mob, std::int32_t colour, std::int32_t flight) = 0;

    /// parity: `MobEffects.HERO_OF_THE_VILLAGE` on a player, its amplifier or nothing.
    [[nodiscard]] virtual std::optional<std::int32_t> heroLevel(std::int32_t playerId) = 0;

    /// What a player holds in their main hand, by item id.
    [[nodiscard]] virtual std::optional<std::string> playerMainHand(std::int32_t playerId) = 0;

    /// parity: `canSeeSky` at a block.
    [[nodiscard]] virtual bool canSeeSky(ServerEntity& mob, const BlockPos& pos) = 0;

    /// parity: the `MOTION_BLOCKING` heightmap at a column.
    [[nodiscard]] virtual std::int32_t motionBlockingHeight(ServerEntity& mob, std::int32_t x, std::int32_t z) = 0;

    /// parity: the `mob_griefing` rule.
    [[nodiscard]] virtual bool mobGriefing(ServerEntity& mob) = 0;

    /// parity: the `overworld_day` timeline's period count, which is what a new day is.
    [[nodiscard]] virtual std::int64_t currentDay(ServerEntity& mob) = 0;

    /// The dropped items within a box around the villager: id, item, count and whether it can be
    /// picked up yet.
    struct ItemView {
        std::int32_t id = 0;
        server::world::loot::ItemStack stack;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        bool pickable = false;
    };

    virtual void itemsNear(ServerEntity& mob, double horizontal, double vertical, std::vector<ItemView>& out) = 0;

    /// Takes up to `count` of a dropped item. Answers how many were taken.
    virtual std::int32_t takeItem(ServerEntity& mob, std::int32_t itemId, std::int32_t count) = 0;
};

namespace villagerBrain {

/// The villager's state, made the first time it is asked for.
VillagerState& stateOf(ServerEntity& villager);

/// One tick of the brain. parity: `Villager.customServerAiStep` over `Brain.tick`.
void tick(ai::MobContext& mob, VillagerWorld& world);

/// parity: `Villager.releaseAllPois`, when it dies or turns into something else.
void releaseAllPois(ai::MobContext& mob, VillagerWorld& world);

/// parity: `Villager.stopSleeping`'s `LAST_WOKEN`.
void wokeUp(ServerEntity& villager, std::int64_t gameTime);

/// parity: `GolemSensor.golemDetected`.
void golemDetected(ServerEntity& villager);

/// parity: `BellBlockEntity`'s `HEARD_BELL_TIME` for everybody within thirty-two blocks of a bell.
void heardBell(ServerEntity& villager, std::int64_t gameTime);

/// parity: `Villager.die`: witnesses remember the murder and the villager lets its spots go.
void died(ai::MobContext& mob, VillagerWorld& world, const std::optional<std::string>& killerUuid);

/// parity: `ReputationEventType.ZOMBIE_VILLAGER_CURED`, the gratitude of a cured villager.
void curedBy(ServerEntity& villager, const std::string& playerUuid);

/// parity: `Villager.getPlayerReputation`.
[[nodiscard]] std::int32_t reputationOf(ServerEntity& villager, const std::string& playerUuid);

/// parity: `Villager.mobInteract` for the hand that trades: answers whether the trading screen opens.
[[nodiscard]] bool startTrading(ServerEntity& villager, VillagerWorld& world, std::int32_t playerId,
                                const std::string& playerUuid, bool mainHand);

/// parity: `Villager.stopTrading`, from the menu closing or the offers running dry.
void stopTrading(ServerEntity& villager);

/// parity: `Villager.rewardTradeXp` for an offer worth `experience`. Answers the orb it drops.
[[nodiscard]] std::int32_t rewardTradeXp(ServerEntity& villager, VillagerWorld& world, std::int32_t experience,
                                         random::RandomSource& random);

/// parity: `Villager.wantsToSpawnGolem`.
[[nodiscard]] bool wantsGolem(ServerEntity& villager, std::int64_t gameTime);

} // namespace villagerBrain

} // namespace basalt::server::runtime::entity::villager
