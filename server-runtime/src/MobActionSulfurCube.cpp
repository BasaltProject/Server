#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/ExplosionSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/entity/MobEquipment.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/SulfurCubes.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/Explosions.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>

#include <cmath>

namespace basalt::server::runtime {

using entity::ServerEntity;
using server::world::loot::ItemStack;
namespace equipment = entity::mobEquipment;
namespace cubes = entity::sulfurCubes;

namespace {

constexpr std::string_view kExplosiveTag = "minecraft:sulfur_cube_archetype/explosive";
constexpr std::string_view kSoundPrimed = "entity.tnt.primed";
constexpr double kExplosionLift = 0.0625;

} // namespace

void MobActionSystem::refreshSulfurBody(ServerEntity& cube) {
    entity::mobMisc::State& misc = entity::mobMisc::of(cube);
    const std::optional<server::world::loot::ItemStack> body = equipment::get(cube, equipment::Slot::Body);
    misc.sulfurBody = body.has_value();
    misc.sulfurExplosive = body.has_value() && itemTagged(kExplosiveTag, body->item);
}

bool MobActionSystem::pluginSulfurCanExplode(ServerEntity& cube) {
    if (!cubes::isSulfurCube(cube)) {
        return false;
    }
    refreshSulfurBody(cube);
    return cubes::canExplode(cube);
}

bool MobActionSystem::pluginSulfurPrime(ServerEntity& cube, bool imminent) {
    world::LoadedWorld* world = worldOf(cube);
    if (world == nullptr || !pluginSulfurCanExplode(cube) || !world->gameRules().get(world::gameRules::kTntExplodes)) {
        return false;
    }
    random::RandomSource random = freshRandom();
    return cubes::prime(cube, imminent, random);
}

void MobActionSystem::tickSulfurCube(world::LoadedWorld& world, ServerEntity& cube) {
    if (cube.removed || cube.dying) {
        return;
    }
    refreshSulfurBody(cube);
    entity::mobMisc::State& misc = entity::mobMisc::of(cube);
    // parity: `SulfurCube.tickFuse`.
    if (cube.fuse > 0) {
        --cube.fuse;
    }
    if (cube.fuse >= 0 && !misc.sulfurFuseSounded) {
        misc.sulfurFuseSounded = true;
        makeSound(world, cube, kSoundPrimed, 1.0F);
    } else if (cube.fuse < 0) {
        misc.sulfurFuseSounded = false;
    }
    if (misc.sulfurExplosive && cube.fuse == 0) {
        cube.removed = true;
        if (world.gameRules().get(world::gameRules::kTntExplodes)) {
            const bool griefing = world.gameRules().get(world::gameRules::kMobGriefing);
            const world::explosions::Spares sparing = [griefing](world::WorldState&, std::int32_t, std::int32_t,
                                                                 std::int32_t, std::int32_t) { return !griefing; };
            explosions_->blowUp(world, watchersOf(world), cube.x, cube.y + kExplosionLift, cube.z,
                                static_cast<float>(cubes::kExplosionPower), "tnt", cube.entityId(), sparing, {}, false);
        }
        return;
    }
    // parity: `SulfurCube.primeWhenOnPoweredPosition`.
    if (cubes::canExplode(cube)) {
        const auto x = static_cast<std::int32_t>(std::floor(cube.x));
        const auto y = static_cast<std::int32_t>(std::floor(cube.y));
        const auto z = static_cast<std::int32_t>(std::floor(cube.z));
        if (world::redstone::bestNeighbourSignal(world.state(), world.name(), x, y, z) > 0) {
            static_cast<void>(pluginSulfurPrime(cube, false));
        }
    }
}

bool MobActionSystem::boostSteerable(ServerEntity& vehicle, std::string_view item) {
    const std::string_view path = vehicle.type().path();
    if (!((path == "pig" && item == "carrot_on_a_stick") ||
          (path == "strider" && item == "warped_fungus_on_a_stick"))) {
        return false;
    }
    entity::mobMisc::State& misc = entity::mobMisc::of(vehicle);
    if (misc.boosting) {
        return false;
    }
    // parity: `ItemBasedSteering.boost`.
    random::RandomSource random = freshRandom();
    misc.boosting = true;
    misc.boostTime = 0;
    misc.boostTotal = random.nextInt(841) + 140;
    return true;
}

void MobActionSystem::layTurtleEgg(ServerEntity& turtle, std::int32_t x, std::int32_t y, std::int32_t z,
                                   std::int32_t eggs) {
    world::LoadedWorld* world = worldOf(turtle);
    const server::registry::BlockState* egg = server::registry::blocks::named("turtle_egg");
    if (world == nullptr || egg == nullptr) {
        return;
    }
    const std::optional<std::int32_t> placed =
        server::registry::blockStateDefinitions::stateIdOf(egg->block, {{"eggs", std::to_string(eggs)}}, egg->stateId);
    const std::optional<std::int32_t> before = world->state().blockState(x, y, z);
    if (!placed.has_value() || !before.has_value()) {
        return;
    }
    random::RandomSource random = freshRandom();
    // parity: `TurtleLayEggGoal.tick`.
    playSound(*world, turtle, "entity.turtle.lay_egg", 0.3F, 0.9F + (random.nextFloat() * 0.2F),
              wire::protocol::play::SoundSource::Blocks);
    if (plugins::hookEvents::changeBlockAllowed(world->events(), world->name(), turtle, x, y, z, *before, *placed,
                                                "lay_egg") &&
        world->state().setBlockState(x, y, z, *placed)) {
        broadcaster_->announceBlock(watchersOf(*world), world::BlockChange{.x = x, .y = y, .z = z, .stateId = *placed});
    }
}

MobActionSystem::SulfurUse MobActionSystem::useOnSulfurCube(ServerEntity& cube, const ItemStack& held) {
    world::LoadedWorld* world = worldOf(cube);
    if (world == nullptr || !cubes::isSulfurCube(cube) || cube.age < 0 || cubes::primed(cube)) {
        return SulfurUse::Pass;
    }
    const std::string_view item = held.item.path();
    // parity: `SulfurCube.mobInteract`, flint and steel or a fire charge lights a cube that can blow.
    if ((item == "flint_and_steel" || item == "fire_charge") && pluginSulfurCanExplode(cube)) {
        if (!pluginSulfurPrime(cube, false)) {
            return SulfurUse::Pass;
        }
        return item == "flint_and_steel" ? SulfurUse::Wear : SulfurUse::Consume;
    }
    if (item == "shears" || !itemTagged("minecraft:sulfur_cube_swallowable", held.item)) {
        return SulfurUse::Pass;
    }
    // parity: `SulfurCube.equipItem`, the old swallowed item spat out unless it is the same one.
    const std::optional<ItemStack> old = equipment::get(cube, equipment::Slot::Body);
    if (old.has_value()) {
        if (old->item == held.item) {
            return SulfurUse::Pass;
        }
        world->state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                    .uuid = wire::common::Uuid::random(),
                                                    .item = old->item,
                                                    .count = old->count,
                                                    .x = cube.x,
                                                    .y = cube.y + (cube.height() * 0.5),
                                                    .z = cube.z,
                                                    .pickupDelay = world::kSpawnedPickupDelay,
                                                    .components = old->components});
    }
    ItemStack one = held;
    one.count = 1;
    equipment::set(cube, equipment::Slot::Body, one, true);
    equipmentChanged(cube, static_cast<std::uint8_t>(equipment::Slot::Body));
    refreshSulfurBody(cube);
    return SulfurUse::Consume;
}

} // namespace basalt::server::runtime
