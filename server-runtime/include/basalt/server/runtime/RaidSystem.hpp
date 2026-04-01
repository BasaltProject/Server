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

/// Raids: the saved `Raids` of each world, each raid's waves and bar, the omens that start them and
/// what a raider does as a member of one.
///
/// parity: `Raids.tick` over `Raid.tick`, `BadOmenMobEffect`, `RaidOmenMobEffect` and `Raider`'s
/// raid half.
class RaidSystem {
public:
    /// What a plugin is told when a raid ends. A callback rather than the plugin bus itself.
    struct Hooks {
        std::function<void(const world::LoadedWorld& world, const entity::Raid& raid, bool won)> raidFinished;
        /// parity: `Stats.RAID_TRIGGER`/`RAID_OMEN` and `Stats.RAID_WIN`/`RAID_WIN`.
        std::function<void(player::PlayerSession& player)> raidTriggered;
        std::function<void(player::PlayerSession& player)> raidWon;
    };

    /// Seeds for each raid's own stream and the hero effect's writer.
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    RaidSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, NaturalSpawningSystem& spawning,
               Hooks hooks, Options options, wire::common::Logger logger = wire::common::Logger{"Raids"});

    /// parity: `ServerLevel.getRaids`.
    [[nodiscard]] static entity::RaidManager& raidsIn(world::LoadedWorld& world);

    /// parity: `ServerLevel.getRaidAt`.
    [[nodiscard]] static entity::Raid* raidAt(world::LoadedWorld& world, double x, double y, double z);

    /// parity: `ServerLevel.getRaidAt`, as the villagers' behaviours see it.
    [[nodiscard]] std::optional<entity::villager::RaidView> viewAt(world::LoadedWorld& world, std::int32_t x,
                                                                   std::int32_t y, std::int32_t z);

    /// parity: `Raids.tick`.
    void tickRaids(world::LoadedWorld& world);

    /// parity: `BadOmenMobEffect` and `RaidOmenMobEffect`'s effect ticks.
    void tickOmens(world::LoadedWorld& world, player::PlayerSession& player, const world::ActiveEffect& active);

    /// parity: `Raids.createOrExtendRaid`.
    entity::Raid* createOrExtendRaid(world::LoadedWorld& world, player::PlayerSession& player,
                                     const entity::RaidPos& position, std::int32_t amplifier);

    /// parity: `Raider.aiStep`'s raid half, before the raider's goals run.
    void tickRaider(world::LoadedWorld& world, entity::ServerEntity& raider);

    /// The raid a raider's goals see. parity: `Raider.getCurrentRaid`.
    [[nodiscard]] std::optional<entity::ai::MobContext::RaiderRaid> raidOf(world::LoadedWorld& world,
                                                                           const entity::ServerEntity& raider);

    /// parity: `Raider.die`: the captain let go, a player killer made a hero, the raider let go.
    void raiderDied(world::LoadedWorld& world, entity::ServerEntity& raider,
                    const std::optional<wire::common::Uuid>& playerKiller);

    /// parity: `PathfindToRaidGoal.recruitNearby`.
    void recruitAround(world::LoadedWorld& world, entity::ServerEntity& raider);

    /// parity: `Raider.pickUpItem`'s banner branch.
    void takeBanner(world::LoadedWorld& world, entity::ServerEntity& raider, std::int32_t itemId);

    /// parity: `Raids.canJoinRaid`.
    [[nodiscard]] static bool canJoinRaid(const entity::ServerEntity& raider);

    /// parity: `Raid.joinRaid` for a raider already in the world.
    void joinRaid(world::LoadedWorld& world, entity::Raid& raid, std::int32_t wave, entity::ServerEntity& raider);

    /// Whether this is a raider at all. parity: `Raider`'s subclasses.
    [[nodiscard]] static bool isRaider(const entity::ServerEntity& mob);

    /// parity: `Raider.setCelebrating`'s own field; the caller syncs it.
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

    /// parity: `Raid.updateBossbar`.
    void updateBossbar(world::LoadedWorld& world, entity::Raid& raid);
    /// The refresh the last hit on one of its raiders asked for, with that raider's health before it.
    void applyRaiderHurts(world::LoadedWorld& world, entity::Raid& raid);
    /// parity: `Raid.updatePlayers`.
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
