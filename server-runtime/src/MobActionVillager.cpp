#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>

namespace basalt::server::runtime {

void MobActionSystem::tickVillager(world::LoadedWorld& world, entity::ServerEntity& villager,
                                   const std::vector<entity::ai::PlayerView>& views,
                                   const std::vector<entity::ServerEntity*>& living, random::RandomSource& random) {
    entity::ai::MobContext context{.world = world.state(),
                                   .mob = villager,
                                   .random = random,
                                   .entities = living,
                                   .players = views,
                                   .actions = *this,
                                   .light = lightAt(world, entity::mobCombat::blockPosition(villager)),
                                   .night = world::beds::isNight(world.clock().totalTicks()),
                                   .dayTime = world.clock().totalTicks(),
                                   .gameTime = world.gameTime(),
                                   .speed = villager.attributeWithModifiers(entity::attribute::kMovementSpeed)};
    entity::villager::villagerBrain::stateOf(villager).dimension =
        config::dimensionRegistryKey(world.declaration().dimension);
    entity::villager::villagerBrain::tick(context, villagersIn(world));
    travelMob(world, villager, context.speed);
}

namespace {

[[nodiscard]] entity::ai::MobContext quietTurn(world::LoadedWorld& world, entity::ServerEntity& villager,
                                               entity::ai::MobActions& actions, random::RandomSource& random) {
    static const std::vector<entity::ServerEntity*> kNobody;
    static const std::vector<entity::ai::PlayerView> kNoPlayers;
    return entity::ai::MobContext{.world = world.state(),
                                  .mob = villager,
                                  .random = random,
                                  .entities = kNobody,
                                  .players = kNoPlayers,
                                  .actions = actions,
                                  .dayTime = world.clock().totalTicks(),
                                  .gameTime = world.gameTime()};
}

} // namespace

void MobActionSystem::villagerDied(world::LoadedWorld& world, entity::ServerEntity& villager,
                                   const std::optional<std::string>& killerUuid) {
    random::RandomSource random = freshRandom();
    entity::ai::MobContext context = quietTurn(world, villager, *this, random);
    entity::villager::villagerBrain::died(context, villagersIn(world), killerUuid);
}

void MobActionSystem::recruitRaiders(entity::ServerEntity& raider) {
    if (world::LoadedWorld* world = worldOf(raider); world != nullptr) {
        raids_->recruitAround(*world, raider);
    }
}

void MobActionSystem::takeRaidBanner(entity::ServerEntity& raider, std::int32_t itemId) {
    if (world::LoadedWorld* world = worldOf(raider); world != nullptr) {
        raids_->takeBanner(*world, raider, itemId);
    }
}

void MobActionSystem::setCelebrating(entity::ServerEntity& raider, bool celebrating) {
    RaidSystem::setCelebrating(raider, celebrating);
    if (world::LoadedWorld* world = worldOf(raider); world != nullptr) {
        // parity: `Raider.IS_CELEBRATING`, index sixteen.
        sendEntityData(
            *world, raider.entityId(),
            wire::protocol::play::SetEntityDataPacket::Entry{
                .index = 16, .value = wire::protocol::play::SetEntityDataPacket::BoolValue{.value = celebrating}});
    }
}

void MobActionSystem::releaseVillagerPois(world::LoadedWorld& world, entity::ServerEntity& villager) {
    random::RandomSource random = freshRandom();
    entity::ai::MobContext context = quietTurn(world, villager, *this, random);
    entity::villager::villagerBrain::releaseAllPois(context, villagersIn(world));
}

} // namespace basalt::server::runtime
