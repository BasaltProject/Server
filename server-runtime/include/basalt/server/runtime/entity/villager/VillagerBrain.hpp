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

struct RaidView {
    bool active = false;
    bool victory = false;
    bool loss = false;
    bool stopped = false;
    bool firstWaveSpawned = false;
    bool betweenWaves = false;
};

struct NodeState {
    bool running = false;
    std::int64_t endTimestamp = 0;
    std::int64_t t0 = 0;
    std::int64_t t1 = 0;
    std::int32_t i0 = 0;
    std::int32_t i1 = 0;
    std::int32_t i2 = 0;
    std::optional<BlockPos> pos;
    std::array<std::uint8_t, 12> order{};
    std::unique_ptr<NodeExtra> extra;
};

struct VillagerState {
    VillagerState();
    VillagerState(const VillagerState&) = delete;
    VillagerState& operator=(const VillagerState&) = delete;
    VillagerState(VillagerState&&) noexcept;
    VillagerState& operator=(VillagerState&&) noexcept;
    ~VillagerState();

    Memories memories;

    struct ForeignPos {
        std::string dimension;
        BlockPos pos;
    };

    std::string dimension;
    std::array<std::string, 4> positionDimensions;
    std::array<std::optional<ForeignPos>, 4> foreignPositions;

    std::uint16_t active = 0;
    Activity defaultActivity = Activity::kIdle;
    std::int64_t lastScheduleUpdate = -9999;
    const BrainTable* table = nullptr;
    std::vector<NodeState> nodes;
    std::array<std::int64_t, 9> sensorClock{};
    bool sensorsStarted = false;

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
    std::array<std::optional<server::world::loot::ItemStack>, 8> inventory;
    std::optional<server::world::loot::ItemStack> mainHand;
    std::optional<std::int32_t> tradingPlayer;
    std::optional<std::int32_t> lastTradedPlayer;
    std::int32_t unhappyCounter = 0;
    bool dataFinalized = false;
    /// Set whenever the profession changes, so the brain is rebuilt before its next turn.
    bool refresh = true;
    bool wasBaby = false;
    std::int32_t seenPanic = 0;
};

class VillagerWorld {
public:
    VillagerWorld() = default;
    VillagerWorld(const VillagerWorld&) = default;
    VillagerWorld(VillagerWorld&&) = default;
    VillagerWorld& operator=(const VillagerWorld&) = default;
    VillagerWorld& operator=(VillagerWorld&&) = default;
    virtual ~VillagerWorld() = default;

    [[nodiscard]] virtual std::optional<RaidView> raidAt(const ServerEntity& villager) = 0;

    virtual void entityEvent(ServerEntity& mob, std::int8_t event) = 0;

    virtual void sound(ServerEntity& mob, std::string_view sound, float volume, float pitch) = 0;

    virtual void blockSound(ServerEntity& mob, const BlockPos& pos, std::string_view sound) = 0;

    virtual void levelEvent(ServerEntity& mob, std::int32_t event, const BlockPos& pos, std::int32_t data) = 0;

    virtual bool setBlock(ServerEntity& mob, const BlockPos& pos, std::int32_t stateId) = 0;

    virtual void destroyBlock(ServerEntity& mob, const BlockPos& pos) = 0;

    virtual void setDoorOpen(ServerEntity& mob, const BlockPos& pos, bool open) = 0;

    virtual void ringBell(ServerEntity& mob, const BlockPos& pos) = 0;

    virtual std::vector<world::trades::Offer>* offers(ServerEntity& mob) = 0;

    virtual std::optional<std::int32_t> composterAdd(ServerEntity& mob, const BlockPos& pos,
                                                     const wire::common::Identifier& item) = 0;

    virtual void composterExtract(ServerEntity& mob, const BlockPos& pos) = 0;

    virtual bool growCrop(ServerEntity& mob, const BlockPos& pos) = 0;

    virtual void throwItem(ServerEntity& mob, const server::world::loot::ItemStack& stack, double towardsX,
                           double towardsY, double towardsZ) = 0;

    virtual void dropItem(ServerEntity& mob, const server::world::loot::ItemStack& stack, float offsetY) = 0;

    virtual void throwGift(ServerEntity& mob, std::string_view lootTable, std::int32_t targetId) = 0;

    /// parity: `LivingEntity.startSleeping` with a bed, `stopSleeping` without one.
    virtual void setSleeping(ServerEntity& mob, std::optional<BlockPos> bed) = 0;

    virtual void villagerDataChanged(ServerEntity& mob) = 0;

    virtual void mainHandChanged(ServerEntity& mob) = 0;

    virtual void unhappyChanged(ServerEntity& mob, std::int32_t counter) = 0;

    virtual void addEffect(ServerEntity& mob, std::string_view effect, std::int32_t duration,
                           std::int32_t amplifier) = 0;

    virtual bool summonGolem(ServerEntity& mob) = 0;

    virtual ServerEntity* spawnChild(ServerEntity& mob, ServerEntity& partner) = 0;

    virtual void launchFirework(ServerEntity& mob, std::int32_t colour, std::int32_t flight) = 0;

    [[nodiscard]] virtual std::optional<std::int32_t> heroLevel(std::int32_t playerId) = 0;

    [[nodiscard]] virtual std::optional<std::string> playerMainHand(std::int32_t playerId) = 0;

    [[nodiscard]] virtual bool canSeeSky(ServerEntity& mob, const BlockPos& pos) = 0;

    [[nodiscard]] virtual std::int32_t motionBlockingHeight(ServerEntity& mob, std::int32_t x, std::int32_t z) = 0;

    [[nodiscard]] virtual bool mobGriefing(ServerEntity& mob) = 0;

    [[nodiscard]] virtual std::int64_t currentDay(ServerEntity& mob) = 0;

    struct ItemView {
        std::int32_t id = 0;
        server::world::loot::ItemStack stack;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        bool pickable = false;
    };

    virtual void itemsNear(ServerEntity& mob, double horizontal, double vertical, std::vector<ItemView>& out) = 0;

    virtual std::int32_t takeItem(ServerEntity& mob, std::int32_t itemId, std::int32_t count) = 0;
};

namespace villagerBrain {

VillagerState& stateOf(ServerEntity& villager);

void tick(ai::MobContext& mob, VillagerWorld& world);

/// parity: `Villager.releaseAllPois`, when it dies or turns into something else.
void releaseAllPois(ai::MobContext& mob, VillagerWorld& world);

void wokeUp(ServerEntity& villager, std::int64_t gameTime);

void golemDetected(ServerEntity& villager);

void heardBell(ServerEntity& villager, std::int64_t gameTime);

void died(ai::MobContext& mob, VillagerWorld& world, const std::optional<std::string>& killerUuid);

void curedBy(ServerEntity& villager, const std::string& playerUuid);

[[nodiscard]] std::int32_t reputationOf(ServerEntity& villager, const std::string& playerUuid);

[[nodiscard]] bool startTrading(ServerEntity& villager, VillagerWorld& world, std::int32_t playerId,
                                const std::string& playerUuid, bool mainHand, bool* talked = nullptr);

void stopTrading(ServerEntity& villager);

[[nodiscard]] std::int32_t rewardTradeXp(ServerEntity& villager, VillagerWorld& world, std::int32_t experience,
                                         random::RandomSource& random);

void tradeUpdated(ServerEntity& villager, VillagerWorld& world, bool validTrade, std::int64_t gameTime);

void noteTrade(ServerEntity& villager, std::int64_t gameTime);

[[nodiscard]] bool wantsGolem(ServerEntity& villager, std::int64_t gameTime);

} // namespace villagerBrain

} // namespace basalt::server::runtime::entity::villager
