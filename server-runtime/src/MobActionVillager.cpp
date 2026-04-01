#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
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
                                   .raidNearby =
                                       raids_->raidsIn(world).nearby(villager.x, villager.y, villager.z) != nullptr,
                                   .speed = villager.attributeWithModifiers(entity::attribute::kMovementSpeed)};
    entity::villager::villagerBrain::tick(context, villagersIn(world));
    travelMob(world, villager, context.speed);
}

} // namespace basalt::server::runtime
