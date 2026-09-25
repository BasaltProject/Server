#pragma once

#include <basalt/server/runtime/world/CustomSpawners.hpp>
#include <basalt/server/runtime/world/MobSpawnForest.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace basalt::server::runtime::player {
class PlayerRegistry;
class PlayerSession;
} // namespace basalt::server::runtime::player

namespace basalt::server::runtime::world {
class LoadedWorld;
class RealBiomeSpawns;
class SpawnEquipmentFinalizer;
} // namespace basalt::server::runtime::world

namespace basalt::server::runtime::sched {
class Region;
}

namespace basalt::server::runtime {

class ChunkSimulationView;

/// What fills the world around the players with the mobs their biomes offer, and how a plugin or
/// another system asks for one mob in particular.
class NaturalSpawningSystem {
public:
    /// What a plugin gets a say in. A callback rather than the plugin bus itself, and unset means
    /// nobody objects.
    struct Hooks {
        std::function<bool(const world::LoadedWorld& world, const wire::common::Identifier& type, double x, double y,
                           double z, std::string_view reason)>
            spawnAllowed;
    };

    /// The streams a spawn attempt draws from: the level's own, and the fresh one every constructed
    /// mob gets.
    struct Options {
        std::function<std::int64_t()> nextSeed;
    };

    NaturalSpawningSystem(player::PlayerRegistry& players, world::RealBiomeSpawns& biomeSpawns,
                          ChunkSimulationView& chunkSimulation, const world::SpawnEquipmentFinalizer& equipment,
                          Hooks hooks, Options options);

    /// One mob of this type where it was asked for, or nothing when the type is not one this build
    /// has.
    ///
    /// `reason` is why it is being made, which is the one thing a plugin always wants to filter on.
    entity::ServerEntity* spawnMob(world::LoadedWorld& world, const wire::common::Identifier& type, double x, double y,
                                   double z, float yaw = 0.0F, std::string_view reason = "natural",
                                   const std::function<void(entity::ServerEntity&)>& initialize = {});

    /// The EVENT horse made by `ServerLevel.tickThunder`; it deliberately skips `finalizeSpawn`.
    entity::ServerEntity* spawnSkeletonTrap(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    /// The TRIGGERED rider forest created when a living player comes within ten blocks.
    bool activateSkeletonTrap(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    /// parity: `EntityType.create` then `finalizeSpawn` with `EVENT` where it was put, as
    /// `Raid.joinRaid` does. `initialize` runs before anybody can see it.
    entity::ServerEntity* spawnEventRaider(world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                           double y, double z, random::RandomSource& random,
                                           const std::function<void(entity::ServerEntity&)>& initialize);

    /// parity: `SpawnUtil.trySpawnMob` with `LEGACY_IRON_GOLEM`, the golem villagers agreed on.
    bool summonVillageGolem(world::LoadedWorld& world, const entity::ServerEntity& villager,
                            random::RandomSource& random);

    /// Fills the world around the players with the mobs their biomes offer.
    ///
    /// parity: `NaturalSpawner.spawnForChunk`, once per simulated chunk per tick, under a cap per
    /// category that scales with how much of the world is being simulated. `doMobSpawning` turns the
    /// whole thing off, as it does in vanilla.
    void tickSpawning(world::LoadedWorld& world, const sched::Region& region);

private:
    /// The five spawners vanilla keeps on a level, which have their own clocks between ticks.
    struct WorldSpawners {
        world::PhantomSpawner phantom;
        world::PatrolSpawner patrol;
        world::CatSpawner cat;
        world::VillageSiegeSpawner villageSiege;
        world::WanderingTraderSpawner wanderingTrader;
    };

    WorldSpawners& spawnersFor(const world::LoadedWorld& world);

    void tickNaturalSpawning(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                             random::RandomSource& random);

    [[nodiscard]] bool spawnAllowed(const world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                    double y, double z, std::string_view reason) const;

    [[nodiscard]] world::MobSpawnEnvironment spawnEnvironment(world::LoadedWorld& world);

    entity::ServerEntity* materializeNaturalForest(world::LoadedWorld& world, const world::SpawnRequest& request,
                                                   std::int32_t blockX, std::int32_t blockY, std::int32_t blockZ,
                                                   random::RandomSource& random);

    entity::ServerEntity* materializeSpawnForest(world::LoadedWorld& world, const world::SpawnRequest& request,
                                                 world::MobSpawnGroupData& group, std::int32_t blockX,
                                                 std::int32_t blockY, std::int32_t blockZ, random::RandomSource& random,
                                                 world::MobSpawnReason reason, std::string_view eventReason);

    entity::ServerEntity* materializePatrolMember(world::LoadedWorld& world, const world::PatrolSpawnRequest& patrol,
                                                  random::RandomSource& random);

    entity::ServerEntity* materializeCat(world::LoadedWorld& world, const world::CatSpawnRequest& cat,
                                         random::RandomSource& random);

    /// VillageSiege finalizes a zero-position zombie and only then moves that zombie, not its
    /// vehicle.
    entity::ServerEntity* materializeVillageSiegeZombie(world::LoadedWorld& world,
                                                        const world::VillageSiegeSpawnRequest& siege,
                                                        random::RandomSource& random);

    entity::ServerEntity* materializeWanderingTrader(world::LoadedWorld& world,
                                                     const world::WanderingTraderSpawnRequest& request,
                                                     random::RandomSource& random);

    void materializeTraderLlama(world::LoadedWorld& world, const world::TraderLlamaSpawnRequest& request,
                                std::int32_t traderId, random::RandomSource& random);

    /// parity: `EntityType.spawn`: level yaw first, then EVENT finalization at the already-positioned
    /// mob.
    entity::ServerEntity* materializeEventMob(world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                              double y, double z, random::RandomSource& random);

    entity::ServerEntity* materializeForest(world::LoadedWorld& world, const world::MobSpawnForest& forest,
                                            std::string_view eventReason = "jockey", bool checkMain = false,
                                            const std::function<void(entity::ServerEntity&)>& initialize = {});

    [[nodiscard]] float specialMultiplierAt(world::LoadedWorld& world, std::int32_t blockX, std::int32_t blockZ) const;

    [[nodiscard]] random::RandomSource freshRandom() const;

    player::PlayerRegistry* players_;
    world::RealBiomeSpawns* biomeSpawns_;
    ChunkSimulationView* chunkSimulation_;
    const world::SpawnEquipmentFinalizer* equipment_;
    Hooks hooks_;
    Options options_;
    std::map<std::string, std::unique_ptr<WorldSpawners>, std::less<>> spawners_;
};

} // namespace basalt::server::runtime
