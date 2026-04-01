#pragma once

#include <basalt/server/runtime/entity/Raids.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/wire/common/logging/Logger.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
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

/// Raids: their waves, the boss bar each one gets, and the bad omen that starts one.
///
/// parity: `Raids` (one per world, kept in memory rather than saved data here) and `Raid` itself.
class RaidSystem {
public:
    /// What a plugin is told when a raid ends. A callback rather than the plugin bus itself.
    struct Hooks {
        std::function<void(const world::LoadedWorld& world, const entity::Raid& raid, bool won)> raidFinished;
    };

    /// Where a wave looks for ground. Its own stream rather than the level's: a raid must not shift
    /// what else spawns that tick.
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    RaidSystem(player::PlayerRegistry& players, BlockChangeBroadcaster& broadcaster, NaturalSpawningSystem& spawning,
               Hooks hooks, Options options, wire::common::Logger logger = wire::common::Logger{"Raids"});

    /// parity: `Raids`, one per world, which this build keeps in memory rather than saved data.
    [[nodiscard]] entity::RaidManager& raidsIn(const world::LoadedWorld& world);

    /// parity: `ServerLevel.getRaidAt`, as the villagers' behaviours see it.
    [[nodiscard]] std::optional<entity::villager::RaidView> viewAt(world::LoadedWorld& world, std::int32_t x,
                                                                   std::int32_t y, std::int32_t z);

    /// Every raid in one world.
    ///
    /// parity: `Raids.tick` over `Raid.tick`: a wave every time the last one is dead, fifteen
    /// seconds apart, and the raid ends when its waves do rather than when the village stops being
    /// one.
    void tickRaids(world::LoadedWorld& world);

    /// parity: `BadOmenMobEffect`, which swaps itself for a raid omen the moment its carrier walks
    /// into a village, and `RaidOmenMobEffect`, which starts the raid where that happened.
    void tickOmens(world::LoadedWorld& world, player::PlayerSession& player, const world::ActiveEffect& active);

private:
    void tickRaid(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& players, entity::Raid& raid);

    /// parity: `spawnGroup`, whose counts are `RaiderType`'s own arrays.
    void spawnRaidWave(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& players,
                       entity::Raid& raid);

    /// parity: `findRandomSpawnPos`, a ring around the village with ground under it.
    [[nodiscard]] std::optional<std::array<std::int32_t, 3>> raidSpawnPosition(world::LoadedWorld& world,
                                                                               const entity::Raid& raid) const;

    /// parity: the victory, which is the hero effect for everybody who was there for it.
    static void winRaid(const std::vector<player::PlayerSession*>& players, entity::Raid& raid);

    static void updateRaidBar(const std::vector<player::PlayerSession*>& players, entity::Raid& raid, double health);

    static void hideRaidBar(const std::vector<player::PlayerSession*>& players, entity::Raid& raid);

    /// parity: `ServerLevel.isVillage`, a section at most one away from a claimed village point.
    [[nodiscard]] static bool isVillage(world::LoadedWorld& world, double x, double y, double z);

    player::PlayerRegistry* players_;
    BlockChangeBroadcaster* broadcaster_;
    NaturalSpawningSystem* spawning_;
    Hooks hooks_;
    Options options_;
    wire::common::Logger logger_;
    std::map<std::string, std::unique_ptr<entity::RaidManager>, std::less<>> managers_;
};

} // namespace basalt::server::runtime
