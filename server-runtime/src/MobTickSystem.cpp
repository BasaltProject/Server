#include <basalt/server/runtime/EntityPushSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/MobTickSystem.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/CopperGolem.hpp>
#include <basalt/server/runtime/entity/EntityCombat.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/MobBrain.hpp>
#include <basalt/server/runtime/entity/MobCategory.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/WanderingTraderLifetime.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/SkeletonHorseTrap.hpp>

#include <algorithm>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::protocol::play::Gamemode;

constexpr double kSpiderPassengerY = 0.765;
constexpr double kAbstractSkeletonVehicleY = 0.7;
constexpr double kWitherSkeletonVehicleY = 0.875;

double passengerAttachment(const ServerEntity& vehicle) {
    if (entity::boats::isBoat(vehicle.type())) {
        return entity::boats::rideHeight(vehicle);
    }
    return vehicle.type().path() == "spider" ? kSpiderPassengerY : vehicle.height();
}

double vehicleAttachment(const ServerEntity& passenger) {
    const std::string_view path = passenger.type().path();
    if (path == "bogged" || path == "parched" || path == "skeleton" || path == "stray") {
        return kAbstractSkeletonVehicleY;
    }
    return path == "wither_skeleton" ? kWitherSkeletonVehicleY : 0.0;
}

} // namespace

namespace entityRiding {

void position(const ServerEntity& vehicle, ServerEntity& passenger) {
    passenger.x = vehicle.x;
    passenger.y = vehicle.y + passengerAttachment(vehicle) - vehicleAttachment(passenger);
    passenger.z = vehicle.z;
}

} // namespace entityRiding

bool MobTickSystem::hasActiveVehicle(world::LoadedWorld& world, ServerEntity& entity) {
    if (!entity.vehicleId.has_value()) {
        return false;
    }
    ServerEntity* vehicle = world.state().entity(*entity.vehicleId);
    if (vehicle != nullptr && !vehicle->removed &&
        std::ranges::find(vehicle->passengerIds, entity.entityId()) != vehicle->passengerIds.end()) {
        return true;
    }
    if (vehicle != nullptr) {
        std::erase(vehicle->passengerIds, entity.entityId());
    }
    entity.vehicleId.reset();
    return false;
}

void MobTickSystem::tickTree(world::LoadedWorld& world, ServerEntity& root, const std::vector<PlayerSession*>& watchers,
                             const std::vector<entity::ai::PlayerView>& views, const std::vector<ServerEntity*>& living,
                             random::RandomSource& random) {
    tickMob(world, root, watchers, views, living, random);
    std::set<std::int32_t> visited{root.entityId()};
    tickPassengers(world, root, watchers, views, living, random, visited);
}

void MobTickSystem::tickPassengers(world::LoadedWorld& world, ServerEntity& vehicle,
                                   const std::vector<PlayerSession*>& watchers,
                                   const std::vector<entity::ai::PlayerView>& views,
                                   const std::vector<ServerEntity*>& living, random::RandomSource& random,
                                   std::set<std::int32_t>& visited) {
    const std::vector<std::int32_t> passengerIds = vehicle.passengerIds;
    for (const std::int32_t passengerId : passengerIds) {
        const PlayerSession* player = nullptr;
        for (const PlayerSession* candidate : watchers) {
            if (candidate->entityId() == passengerId) {
                player = candidate;
                break;
            }
        }
        if (player != nullptr) {
            if (!player->vehicleId.has_value() || *player->vehicleId != vehicle.entityId()) {
                std::erase(vehicle.passengerIds, passengerId);
            }
            continue;
        }
        ServerEntity* passenger = world.state().entity(passengerId);
        if (passenger == nullptr || passenger->removed || !passenger->vehicleId.has_value() ||
            *passenger->vehicleId != vehicle.entityId() || !visited.insert(passengerId).second) {
            std::erase(vehicle.passengerIds, passengerId);
            if (passenger != nullptr && passenger->vehicleId.has_value() &&
                *passenger->vehicleId == vehicle.entityId()) {
                passenger->vehicleId.reset();
            }
            continue;
        }
        passenger->velocityX = 0.0;
        passenger->velocityY = 0.0;
        passenger->velocityZ = 0.0;
        if (entity::mobs::isMob(passenger->type())) {
            tickMob(world, *passenger, watchers, views, living, random);
        }
        entityRiding::position(vehicle, *passenger);
        tickPassengers(world, *passenger, watchers, views, living, random, visited);
    }
}

void MobTickSystem::tickMob(world::LoadedWorld& world, ServerEntity& entity,
                            const std::vector<PlayerSession*>& watchers,
                            const std::vector<entity::ai::PlayerView>& views, const std::vector<ServerEntity*>& living,
                            random::RandomSource& random) {
    entity.idleTicks++;
    entity.tickCount++;
    // parity: `Entity.baseTick`, the boarding cooldown a dismount leaves.
    if (entity.boardingCooldown > 0) {
        --entity.boardingCooldown;
    }
    pushes_->pushByFluid(world, entity);
    // parity: `Entity.baseTick` ends in `Leashable.tickLeash`, before the mob's own `aiStep`.
    actions_->tickLeash(world, entity);
    if (entity.removed) {
        return;
    }
    entity::entityCombat::tick(entity);
    actions_->tickAmbientSound(world, entity, random);
    // parity: `LivingEntity.tick`.
    if (entity.arrowCount > 0) {
        if (entity.removeArrowTime <= 0) {
            entity.removeArrowTime = 20 * (30 - entity.arrowCount);
        }
        if (--entity.removeArrowTime <= 0) {
            --entity.arrowCount;
        }
    }
    if (entity.stingerCount > 0) {
        if (entity.removeStingerTime <= 0) {
            entity.removeStingerTime = 20 * (30 - entity.stingerCount);
        }
        if (--entity.removeStingerTime <= 0) {
            --entity.stingerCount;
        }
    }
    if (entity::wardens::isWarden(entity.type())) {
        actions_->tickWarden(world, watchers, entity);
        if (entity.removed) {
            return;
        }
    }
    if (entity::copperGolems::isCopperGolem(entity.type())) {
        actions_->tickCopperGolem(world, entity, random);
        if (entity.removed) {
            return;
        }
    }
    if (entity.panicTicks > 0) {
        entity.panicTicks--;
    }
    if (entity.type().path() == "vex") {
        actions_->tickVexLife(watchers, entity);
    }
    actions_->tickFire(world, watchers, entity, random);
    actions_->tickMobEffects(world, watchers, entity);
    actions_->tickBreath(world, watchers, entity, random);
    if (entity.type().path() == "ravager") {
        actions_->tickRavager(world, watchers, entity);
    }
    if (actions_->tickConversion(world, watchers, entity)) {
        return;
    }
    // parity: `SnowGolem.aiStep` is the living entity's own turn.
    // even where nothing is driving it.
    if (entity.type().path() == "snow_golem") {
        actions_->trailSnow(world, entity);
    }
    if (entity.type().path() == "skeleton_horse" && entity.skeletonTrap) {
        const double rangeSquared = world::skeletonHorseTrap::kPlayerRange * world::skeletonHorseTrap::kPlayerRange;
        bool nearbyAlivePlayer = false;
        if (!entity.noAi) {
            for (const PlayerSession* player : watchers) {
                if (player->dead || player->gamemode == Gamemode::Spectator) {
                    continue;
                }
                const double dx = player->x - entity.x;
                const double dy = player->y - entity.y;
                const double dz = player->z - entity.z;
                if ((dx * dx) + (dy * dy) + (dz * dz) <= rangeSquared) {
                    nearbyAlivePlayer = true;
                    break;
                }
            }
        }
        if (nearbyAlivePlayer) {
            actions_->activateSkeletonTrap(world, entity, random);
        }
        world::skeletonHorseTrap::tickLifetime(entity);
        if (entity.removed) {
            return;
        }
    }
    // parity: a saddled mount a player steers moves by that player's client, not by its own goals.
    const bool steered = entity::horses::isHorse(entity.type()) && entity::horses::saddled(entity) &&
                         !entity.passengerIds.empty() &&
                         std::ranges::any_of(watchers, [&entity](const PlayerSession* player) {
                             return player->entityId() == entity.passengerIds.front();
                         });
    if (!entity.noAi && !steered) {
        actions_->tickMobAi(world, entity, views, living, random);
        if (entity.type().path() == "dolphin") {
            actions_->dolphinPicksUp(world, entity);
        }
        ServerEntity* partner = entity::breeding::tick(entity, living);
        if (partner != nullptr) {
            actions_->breed(world, entity, *partner);
        }
    }
    if (entity::horses::isHorse(entity.type())) {
        actions_->tickHorse(world, entity, random);
    }
    if (steered && entity::camels::isCamel(entity.type())) {
        for (const PlayerSession* rider : watchers) {
            if (rider->entityId() == entity.passengerIds.front()) {
                actions_->camelRidden(world, entity, *rider);
            }
        }
    }
    // parity: `Bee.customServerAiStep` and the rest of `Bee.aiStep`, after the goals.
    if (entity::bees::isBee(entity.type())) {
        actions_->tickBee(world, watchers, entity, random);
    }
    if (entity.type().path() == "zombified_piglin") {
        actions_->tickZombifiedPiglin(world, entity, random);
    }
    if (const std::string_view kind = entity.type().path();
        kind == "wolf" || kind == "iron_golem" || kind == "polar_bear" || kind == "enderman") {
        actions_->tickNeutralMob(world, entity, random);
    }
    if (entity.type().path() == "parrot") {
        actions_->tickParrot(world, entity);
    }
    // parity: `Pufferfish.tick` and `aiStep`.
    // the travel they belong to.
    if (entity.type().path() == "pufferfish") {
        actions_->tickPufferfish(world, watchers, entity, living);
    }
    // parity: `LivingEntity.aiStep`'s `pushEntities`, after the travel.
    pushes_->pushByMob(world, entity);
    actions_->emitMovement(world, entity);
    if (entity.removed) {
        return;
    }
    // parity: `Shulker.tick`, after the living entity's own turn.
    if (entity::shulkers::isShulker(entity.type())) {
        actions_->tickShulker(world, watchers, living, entity, random);
    }
    const std::string_view path = entity.type().path();
    if (path == "wandering_trader" || path == "trader_llama") {
        std::set<std::int32_t> playerIds;
        for (const PlayerSession* player : watchers) {
            playerIds.insert(player->entityId());
        }
        entity::wanderingTraderLifetime::tick(entity, living, playerIds);
        if (entity.removed) {
            return;
        }
    }
    const entity::MobCategory category =
        entity::mobCategoryByName(entity::mobs::categoryOf(entity.type())).value_or(entity::MobCategory::Misc);
    std::optional<double> nearest;
    for (const PlayerSession* player : watchers) {
        const double distance = entity::mobBrain::distanceTo(entity, player->x, player->y, player->z);
        if (!nearest.has_value() || distance < *nearest) {
            nearest = distance;
        }
    }
    if (entity::mobDespawn::shouldDespawn(entity, category, nearest, random)) {
        entity.removed = true;
    }
}

} // namespace basalt::server::runtime
