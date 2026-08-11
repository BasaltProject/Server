#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/wire/common/Uuid.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealLootTables;
class RealVillagerTrades;
struct ActiveEffect;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class NaturalSpawningSystem;
class RaidSystem;

class VillagerWorldSystem final : public entity::villager::VillagerWorld {
public:
    /// parity: `LivingEntity.addEffect`, which the mob machinery owns.
    using AddEffect = std::function<void(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                                         entity::ServerEntity& mob, const world::ActiveEffect& effect)>;

    using LaunchFirework = std::function<void(world::LoadedWorld& world, wire::nbt::NbtCompound item, double x,
                                              double y, double z, std::int32_t ownerId)>;

    using Explode =
        std::function<void(world::LoadedWorld& world, double x, double y, double z, float power, bool fire)>;

    VillagerWorldSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster,
                        NaturalSpawningSystem& spawning, RaidSystem& raids, world::RealLootTables& lootTables,
                        world::RealVillagerTrades* trades, std::function<std::int64_t()> nextSeed,
                        AddEffect addEffect = {}, LaunchFirework launchFirework = {}, Explode explode = {});

    void bind(world::LoadedWorld& world) { world_ = &world; }

    [[nodiscard]] std::optional<entity::villager::RaidView> raidAt(const entity::ServerEntity& villager) override;
    void entityEvent(entity::ServerEntity& mob, std::int8_t event) override;
    void sound(entity::ServerEntity& mob, std::string_view sound, float volume, float pitch) override;
    void blockSound(entity::ServerEntity& mob, const entity::villager::BlockPos& pos, std::string_view sound) override;
    void levelEvent(entity::ServerEntity& mob, std::int32_t event, const entity::villager::BlockPos& pos,
                    std::int32_t data) override;
    bool setBlock(entity::ServerEntity& mob, const entity::villager::BlockPos& pos, std::int32_t stateId) override;
    void destroyBlock(entity::ServerEntity& mob, const entity::villager::BlockPos& pos) override;
    void setDoorOpen(entity::ServerEntity& mob, const entity::villager::BlockPos& pos, bool open) override;
    void ringBell(entity::ServerEntity& mob, const entity::villager::BlockPos& pos) override;
    std::vector<world::trades::Offer>* offers(entity::ServerEntity& mob) override;
    std::optional<std::int32_t> composterAdd(entity::ServerEntity& mob, const entity::villager::BlockPos& pos,
                                             const wire::common::Identifier& item) override;
    void composterExtract(entity::ServerEntity& mob, const entity::villager::BlockPos& pos) override;
    bool growCrop(entity::ServerEntity& mob, const entity::villager::BlockPos& pos) override;
    void throwItem(entity::ServerEntity& mob, const server::world::loot::ItemStack& stack, double towardsX,
                   double towardsY, double towardsZ) override;
    void dropItem(entity::ServerEntity& mob, const server::world::loot::ItemStack& stack, float offsetY) override;
    void throwGift(entity::ServerEntity& mob, std::string_view lootTable, std::int32_t targetId) override;
    void setSleeping(entity::ServerEntity& mob, std::optional<entity::villager::BlockPos> bed) override;
    void villagerDataChanged(entity::ServerEntity& mob) override;
    void mainHandChanged(entity::ServerEntity& mob) override;
    void unhappyChanged(entity::ServerEntity& mob, std::int32_t counter) override;
    void addEffect(entity::ServerEntity& mob, std::string_view effect, std::int32_t duration,
                   std::int32_t amplifier) override;
    bool summonGolem(entity::ServerEntity& mob) override;
    entity::ServerEntity* spawnChild(entity::ServerEntity& mob, entity::ServerEntity& partner) override;
    void launchFirework(entity::ServerEntity& mob, std::int32_t colour, std::int32_t flight) override;
    [[nodiscard]] std::optional<std::int32_t> heroLevel(std::int32_t playerId) override;
    [[nodiscard]] std::optional<std::string> playerMainHand(std::int32_t playerId) override;
    [[nodiscard]] bool canSeeSky(entity::ServerEntity& mob, const entity::villager::BlockPos& pos) override;
    [[nodiscard]] std::int32_t motionBlockingHeight(entity::ServerEntity& mob, std::int32_t x, std::int32_t z) override;
    [[nodiscard]] bool mobGriefing(entity::ServerEntity& mob) override;
    [[nodiscard]] std::int64_t currentDay(entity::ServerEntity& mob) override;
    void itemsNear(entity::ServerEntity& mob, double horizontal, double vertical, std::vector<ItemView>& out) override;
    std::int32_t takeItem(entity::ServerEntity& mob, std::int32_t itemId, std::int32_t count) override;

    static void alertVillagers(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

private:
    [[nodiscard]] std::vector<player::PlayerSession*> watchers() const;
    [[nodiscard]] player::PlayerSession* playerById(std::int32_t playerId) const;
    void spawnItem(const server::world::loot::ItemStack& stack, double x, double y, double z, double velocityX,
                   double velocityY, double velocityZ, std::optional<wire::common::Uuid> thrower = std::nullopt);
    void announce(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId, bool neighbours);

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    NaturalSpawningSystem* spawning_;
    RaidSystem* raids_;
    world::RealLootTables* lootTables_;
    world::RealVillagerTrades* trades_;
    std::function<std::int64_t()> nextSeed_;
    AddEffect addEffect_;
    LaunchFirework launchFirework_;
    Explode explode_;
    world::LoadedWorld* world_ = nullptr;
};

} // namespace basalt::server::runtime
