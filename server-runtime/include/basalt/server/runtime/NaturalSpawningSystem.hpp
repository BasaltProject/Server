#pragma once

#include <basalt/server/runtime/world/CustomSpawners.hpp>
#include <basalt/server/runtime/world/MobSpawnForest.hpp>
#include <basalt/server/runtime/world/NaturalSpawner.hpp>
#include <basalt/wire/common/Identifier.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
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

class NaturalSpawningSystem {
public:
    struct Hooks {
        std::function<bool(const world::LoadedWorld& world, const wire::common::Identifier& type, double x, double y,
                           double z, std::string_view reason)>
            spawnAllowed;
        std::function<void(const world::LoadedWorld& world, const entity::ServerEntity& entity,
                           std::string_view reason)>
            spawned;
    };

    struct Options {
        std::function<std::int64_t()> nextSeed;
        /// Members of a biome tag such as `minecraft:is_forest`, for the `spawn-biomes` overrides.
        std::function<std::set<std::string>(const std::string&)> biomeTag;
    };

    NaturalSpawningSystem(player::PlayerRegistry& players, world::RealBiomeSpawns& biomeSpawns,
                          ChunkSimulationView& chunkSimulation, const world::SpawnEquipmentFinalizer& equipment,
                          Hooks hooks, Options options);

    /// Asks the entity.spawn event for a spawn that does not come through the spawner, for the plugin API.
    [[nodiscard]] bool askSpawn(const world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                double y, double z, std::string_view reason) const {
        return spawnAllowed(world, type, x, y, z, reason);
    }

    entity::ServerEntity* spawnMob(world::LoadedWorld& world, const wire::common::Identifier& type, double x, double y,
                                   double z, float yaw = 0.0F, std::string_view reason = "natural",
                                   const std::function<void(entity::ServerEntity&)>& initialize = {});

    [[nodiscard]] world::MobSpawnForest
    finalizeForSpawner(world::LoadedWorld& world, const wire::common::Identifier& type, double x, double y, double z,
                       float yaw, random::RandomSource& random,
                       world::MobSpawnReason reason = world::MobSpawnReason::Spawner);

    entity::ServerEntity* spawnSkeletonTrap(world::LoadedWorld& world, std::int32_t x, std::int32_t y, std::int32_t z);

    bool activateSkeletonTrap(world::LoadedWorld& world, entity::ServerEntity& horse, random::RandomSource& random);

    entity::ServerEntity* spawnEventRaider(world::LoadedWorld& world, const wire::common::Identifier& type, double x,
                                           double y, double z, random::RandomSource& random,
                                           const std::function<void(entity::ServerEntity&)>& initialize);

    bool summonVillageGolem(world::LoadedWorld& world, const entity::ServerEntity& villager,
                            random::RandomSource& random);

    void tickSpawning(world::LoadedWorld& world, const sched::Region& region);

private:
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

    entity::ServerEntity* materializeVillageSiegeZombie(world::LoadedWorld& world,
                                                        const world::VillageSiegeSpawnRequest& siege,
                                                        random::RandomSource& random);

    entity::ServerEntity* materializeWanderingTrader(world::LoadedWorld& world,
                                                     const world::WanderingTraderSpawnRequest& request,
                                                     random::RandomSource& random);

    void materializeTraderLlama(world::LoadedWorld& world, const world::TraderLlamaSpawnRequest& request,
                                std::int32_t traderId, random::RandomSource& random);

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
