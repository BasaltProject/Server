#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/MobSpawnFinalization.hpp>

#include <cmath>
#include <cstdint>
#include <utility>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using wire::common::Identifier;

constexpr float kFullRotation = 360.0F;
constexpr std::int32_t kWanderingTraderDespawnDelay = 48000;
constexpr std::int32_t kWanderingTraderHomeRadius = 16;

} // namespace

ServerEntity* NaturalSpawningSystem::materializeNaturalForest(world::LoadedWorld& world,
                                                              const world::SpawnRequest& request, std::int32_t blockX,
                                                              std::int32_t blockY, std::int32_t blockZ,
                                                              random::RandomSource& random) {
    world::MobSpawnGroupData group;
    return materializeSpawnForest(world, request, group, blockX, blockY, blockZ, random, world::MobSpawnReason::Natural,
                                  "natural");
}

ServerEntity* NaturalSpawningSystem::materializeSpawnForest(world::LoadedWorld& world,
                                                            const world::SpawnRequest& request,
                                                            world::MobSpawnGroupData& group, std::int32_t blockX,
                                                            std::int32_t blockY, std::int32_t blockZ,
                                                            random::RandomSource& random, world::MobSpawnReason reason,
                                                            std::string_view eventReason) {
    if (!spawnAllowed(world, request.type, request.x, request.y, request.z, eventReason)) {
        return nullptr;
    }
    const world::MobSpawnContext context{.random = &random,
                                         .specialMultiplier = specialMultiplierAt(world, blockX, blockZ),
                                         .difficulty = world.difficulty(),
                                         .equipment = equipment_,
                                         .biome = request.biome,
                                         .gameTime = world.gameTime(),
                                         .reason = reason,
                                         .entityRandomFactory = [this] { return freshRandom(); }};
    wire::nbt::NbtCompound data;
    const world::MobSpawnForest forest =
        world::writeGeneratedForest(data, request.type, request.groupIndex, context, group, blockX, blockY, blockZ,
                                    request.x, request.y, request.z, request.yaw, spawnEnvironment(world));
    return materializeForest(world, forest);
}

ServerEntity* NaturalSpawningSystem::materializePatrolMember(world::LoadedWorld& world,
                                                             const world::PatrolSpawnRequest& patrol,
                                                             random::RandomSource& random) {
    world::MobSpawnGroupData group;
    group.patrolLeader = patrol.leader;
    const world::SpawnRequest request{.type = Identifier::vanilla("pillager"),
                                      .x = patrol.x,
                                      .y = patrol.y,
                                      .z = patrol.z,
                                      .yaw = 0.0F,
                                      .biome = patrol.biome};
    return materializeSpawnForest(world, request, group, static_cast<std::int32_t>(std::floor(patrol.x)),
                                  static_cast<std::int32_t>(std::floor(patrol.y)),
                                  static_cast<std::int32_t>(std::floor(patrol.z)), random,
                                  world::MobSpawnReason::Patrol, "patrol");
}

ServerEntity* NaturalSpawningSystem::materializeCat(world::LoadedWorld& world, const world::CatSpawnRequest& cat,
                                                    random::RandomSource& random) {
    world::MobSpawnGroupData group;
    group.catSpawnContext = cat.context;
    const world::SpawnRequest request{
        .type = Identifier::vanilla("cat"), .x = cat.x, .y = cat.y, .z = cat.z, .yaw = 0.0F, .biome = cat.biome};
    ServerEntity* spawned = materializeSpawnForest(world, request, group, static_cast<std::int32_t>(std::floor(cat.x)),
                                                   static_cast<std::int32_t>(std::floor(cat.y)),
                                                   static_cast<std::int32_t>(std::floor(cat.z)), random,
                                                   world::MobSpawnReason::Natural, "natural");
    if (spawned != nullptr) {
        spawned->persistent = cat.persistent;
    }
    return spawned;
}

ServerEntity* NaturalSpawningSystem::materializeVillageSiegeZombie(world::LoadedWorld& world,
                                                                   const world::VillageSiegeSpawnRequest& siege,
                                                                   random::RandomSource& random) {
    const Identifier zombie = Identifier::vanilla("zombie");
    if (!spawnAllowed(world, zombie, siege.x, siege.y, siege.z, "event")) {
        return nullptr;
    }
    // parity: `VillageSiege` finalizes the zombie at the origin and only then moves it, which is
    // what keeps whatever it is riding where the finalizer put it.
    const world::MobSpawnContext context{.random = &random,
                                         .specialMultiplier = specialMultiplierAt(world, 0, 0),
                                         .difficulty = world.difficulty(),
                                         .equipment = equipment_,
                                         .biome = world::naturalSpawner::biomeAt(world, 0, 0, 0),
                                         .gameTime = world.gameTime(),
                                         .reason = world::MobSpawnReason::Event,
                                         .entityRandomFactory = [this] { return freshRandom(); }};
    wire::nbt::NbtCompound data;
    world::MobSpawnGroupData group;
    const world::MobSpawnForest forest = world::writeGeneratedForest(data, zombie, 0, context, group, 0, 0, 0, 0.0, 0.0,
                                                                     0.0, 0.0F, spawnEnvironment(world));
    const float yaw = random.nextFloat() * kFullRotation;
    return materializeForest(world, world::relocateMain(forest, siege.x, siege.y, siege.z, yaw));
}

ServerEntity* NaturalSpawningSystem::materializeWanderingTrader(world::LoadedWorld& world,
                                                                const world::WanderingTraderSpawnRequest& request,
                                                                random::RandomSource& random) {
    ServerEntity* trader =
        materializeEventMob(world, Identifier::vanilla("wandering_trader"), request.x, request.y, request.z, random);
    if (trader == nullptr) {
        return nullptr;
    }
    trader->despawnDelay = kWanderingTraderDespawnDelay;
    trader->wanderTarget = std::array<std::int32_t, 3>{request.referenceX, request.referenceY, request.referenceZ};
    trader->homePosition = std::array<std::int32_t, 3>{request.referenceX, request.referenceY, request.referenceZ};
    trader->homeRadius = kWanderingTraderHomeRadius;
    return trader;
}

void NaturalSpawningSystem::materializeTraderLlama(world::LoadedWorld& world,
                                                   const world::TraderLlamaSpawnRequest& request, std::int32_t traderId,
                                                   random::RandomSource& random) {
    ServerEntity* trader = world.state().entity(traderId);
    if (trader == nullptr || trader->removed) {
        return;
    }
    ServerEntity* llama =
        materializeEventMob(world, Identifier::vanilla("trader_llama"), request.x, request.y, request.z, random);
    if (llama == nullptr) {
        return;
    }
    llama->leashHolderId = trader->entityId();
    llama->leashHolderUuid = trader->uuid().toString();
}

ServerEntity* NaturalSpawningSystem::materializeEventMob(world::LoadedWorld& world, const Identifier& type, double x,
                                                         double y, double z, random::RandomSource& random) {
    if (!spawnAllowed(world, type, x, y, z, "event")) {
        return nullptr;
    }
    const auto blockX = static_cast<std::int32_t>(std::floor(x));
    const auto blockY = static_cast<std::int32_t>(std::floor(y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(z));
    const float yaw = random.nextFloat() * kFullRotation;
    const world::MobSpawnContext context{.random = &random,
                                         .specialMultiplier = specialMultiplierAt(world, blockX, blockZ),
                                         .difficulty = world.difficulty(),
                                         .equipment = equipment_,
                                         .biome = world::naturalSpawner::biomeAt(world, blockX, blockY, blockZ),
                                         .gameTime = world.gameTime(),
                                         .reason = world::MobSpawnReason::Event,
                                         .entityRandomFactory = [this] { return freshRandom(); }};
    wire::nbt::NbtCompound data;
    world::MobSpawnGroupData group;
    const world::MobSpawnForest forest = world::writeGeneratedForest(data, type, 0, context, group, blockX, blockY,
                                                                     blockZ, x, y, z, yaw, spawnEnvironment(world));
    return materializeForest(world, forest);
}

ServerEntity* NaturalSpawningSystem::materializeForest(world::LoadedWorld& world, const world::MobSpawnForest& forest,
                                                       std::string_view eventReason, bool checkMain,
                                                       const std::function<void(ServerEntity&)>& initialize) {
    ServerEntity* main = nullptr;
    // Recursive rather than iterative because a passenger tree is one: each mob is spawned before
    // whatever rides it, so the rider has a vehicle id to point at.
    const auto materialize = [&](auto&& self, const server::world::ChunkGeneratedEntity& descriptor,
                                 ServerEntity* vehicle) -> void {
        const bool isMain = &descriptor == &forest.main;
        if ((!isMain || checkMain) &&
            !spawnAllowed(world, descriptor.type, descriptor.x, descriptor.y, descriptor.z, eventReason)) {
            return;
        }
        const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(descriptor.type);
        if (entry == nullptr) {
            return;
        }
        std::unique_ptr<ServerEntity> decoded =
            entity::entityNbt::decodeGenerated(descriptor, players_->allocateEntityId(), entry->protocolId);
        if (decoded == nullptr) {
            return;
        }
        ServerEntity* resolvedVehicle = vehicle;
        if (resolvedVehicle == nullptr) {
            for (const world::MobExternalVehicleLink& link : forest.externalVehicleLinks) {
                if (&link.passenger == &descriptor) {
                    resolvedVehicle = world.state().entity(link.vehicleId);
                    break;
                }
            }
        }
        if (resolvedVehicle != nullptr) {
            decoded->vehicleId = resolvedVehicle->entityId();
            resolvedVehicle->passengerIds.push_back(decoded->entityId());
            if (resolvedVehicle->type().path() == "chicken") {
                if (!resolvedVehicle->sourceNbt.has_value()) {
                    resolvedVehicle->sourceNbt = wire::nbt::NbtCompound{};
                }
                resolvedVehicle->sourceNbt->putBoolean("IsChickenJockey", true);
            }
        }
        if (initialize) {
            initialize(*decoded);
        }
        ServerEntity& spawned = world.state().spawnEntity(std::move(decoded));
        if (isMain) {
            main = &spawned;
        }
        for (const server::world::ChunkGeneratedEntity& passenger : descriptor.passengers) {
            self(self, passenger, &spawned);
        }
    };
    for (const server::world::ChunkGeneratedEntity& root : forest.roots) {
        materialize(materialize, root, nullptr);
    }
    return main;
}

} // namespace basalt::server::runtime
