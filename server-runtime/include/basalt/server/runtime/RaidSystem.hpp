#pragma once

#include <basalt/server/runtime/entity/Raids.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
struct ActiveEffect;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime {

class BlockChangeBroadcaster;
class NaturalSpawningSystem;

class RaidSystem {
public:
    struct Hooks {
        std::function<void(const world::LoadedWorld& world, const entity::Raid& raid, bool won)> raidFinished;
        std::function<void(player::PlayerSession& player)> raidTriggered;
        std::function<void(player::PlayerSession& player)> raidWon;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    RaidSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, NaturalSpawningSystem& spawning,
               Hooks hooks, Options options, wire::common::Logger logger = wire::common::Logger{"Raids"});

    [[nodiscard]] static entity::RaidManager& raidsIn(world::LoadedWorld& world);

    [[nodiscard]] static entity::Raid* raidAt(world::LoadedWorld& world, double x, double y, double z);

    [[nodiscard]] std::optional<entity::villager::RaidView> viewAt(world::LoadedWorld& world, std::int32_t x,
                                                                   std::int32_t y, std::int32_t z);

    void tickRaids(world::LoadedWorld& world);

    void tickOmens(world::LoadedWorld& world, player::PlayerSession& player, const world::ActiveEffect& active);

    entity::Raid* createOrExtendRaid(world::LoadedWorld& world, player::PlayerSession& player,
                                     const entity::RaidPos& position, std::int32_t amplifier);

    /// parity: `Raider.aiStep`'s raid half, before the raider's goals run.
    void tickRaider(world::LoadedWorld& world, entity::ServerEntity& raider);

    [[nodiscard]] std::optional<entity::ai::MobContext::RaiderRaid> raidOf(world::LoadedWorld& world,
                                                                           const entity::ServerEntity& raider);

    void raiderDied(world::LoadedWorld& world, entity::ServerEntity& raider,
                    const std::optional<wire::common::Uuid>& playerKiller);

    void recruitAround(world::LoadedWorld& world, entity::ServerEntity& raider);

    void takeBanner(world::LoadedWorld& world, entity::ServerEntity& raider, std::int32_t itemId);

    [[nodiscard]] static bool canJoinRaid(const entity::ServerEntity& raider);

    void joinRaid(world::LoadedWorld& world, entity::Raid& raid, std::int32_t wave, entity::ServerEntity& raider);

    [[nodiscard]] static bool isRaider(const entity::ServerEntity& mob);

    /// What a plugin asks of a raid's boss bar: title (component JSON), color, style, flags, progress, who sees it.
    bool controlBar(world::LoadedWorld& world, std::int32_t id, std::string_view action, std::string_view text,
                    std::int32_t value);

    static void setCelebrating(entity::ServerEntity& raider, bool celebrating);

private:
    void tickRaid(world::LoadedWorld& world, entity::Raid& raid);
    void tickOngoing(world::LoadedWorld& world, entity::Raid& raid);
    void tickCelebration(world::LoadedWorld& world, entity::Raid& raid);
    void spawnGroup(world::LoadedWorld& world, entity::Raid& raid, const entity::RaidPos& pos);
    [[nodiscard]] entity::ServerEntity* spawnRaider(world::LoadedWorld& world, entity::Raid& raid,
                                                    std::string_view type, std::int32_t wave,
                                                    const entity::RaidPos& pos);
    void applyRaidBuffs(entity::Raid& raid, entity::ServerEntity& raider, std::int32_t wave);
    [[nodiscard]] std::optional<entity::RaidPos> findRandomSpawnPos(world::LoadedWorld& world, entity::Raid& raid,
                                                                    std::int32_t maxTries);
    void playHorn(world::LoadedWorld& world, entity::Raid& raid, const entity::RaidPos& origin);
    void updateRaiders(world::LoadedWorld& world, entity::Raid& raid);
    void removeFromRaid(world::LoadedWorld& world, entity::Raid& raid, entity::ServerEntity& raider,
                        bool fromTotalHealth);
    void setLeader(world::LoadedWorld& world, entity::Raid& raid, std::int32_t wave, entity::ServerEntity& raider);
    void rewardHeroes(world::LoadedWorld& world, entity::Raid& raid);
    static void releaseRaiders(world::LoadedWorld& world, entity::Raid& raid);
    static void moveCenterToVillageSection(world::LoadedWorld& world, entity::Raid& raid);

    void updateBossbar(world::LoadedWorld& world, entity::Raid& raid);
    /// The refresh the last hit on one of its raiders asked for, with that raider's health before it.
    void applyRaiderHurts(world::LoadedWorld& world, entity::Raid& raid);
    void updatePlayers(world::LoadedWorld& world, entity::Raid& raid);
    void setBarName(world::LoadedWorld& world, entity::Raid& raid, wire::text::Component name);
    void setBarProgress(world::LoadedWorld& world, entity::Raid& raid, float progress);
    void setBarVisible(world::LoadedWorld& world, entity::Raid& raid, bool visible);
    void removeAllBarPlayers(world::LoadedWorld& world, entity::Raid& raid);
    void sendEquipment(world::LoadedWorld& world, const entity::ServerEntity& raider);

    [[nodiscard]] std::vector<player::PlayerSession*> playersIn(const world::LoadedWorld& world) const;

    [[nodiscard]] static bool isVillage(world::LoadedWorld& world, const entity::RaidPos& pos);

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    NaturalSpawningSystem* spawning_;
    Hooks hooks_;
    Options options_;
    wire::common::Logger logger_;
};

} // namespace basalt::server::runtime
