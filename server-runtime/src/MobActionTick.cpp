#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/MobGoals.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace basalt::server::runtime {
namespace {

using entity::ServerEntity;
using player::PlayerSession;
using wire::common::Identifier;
using wire::protocol::play::SetEntityDataPacket;

/// parity: `canSeeSky`, which here is the full fifteen of sky light above the mob.
constexpr std::int32_t kSkyLightMax = 15;

} // namespace

void MobActionSystem::tickFire(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                               ServerEntity& mob, random::RandomSource& random) {
    const bool wasBurning = mob.fireTicks > 0;
    const auto blockX = static_cast<std::int32_t>(std::floor(mob.x));
    const auto blockY = static_cast<std::int32_t>(std::floor(mob.y));
    const auto blockZ = static_cast<std::int32_t>(std::floor(mob.z));
    if (entity::sunBurn::burns(mob.type())) {
        const bool day = !world::beds::isNight(world.clock().totalTicks());
        const std::int32_t light = world::worldLight::skyLight(world.state(), blockX, blockY, blockZ);
        const bool openSky = world::worldLight::skyLight(world.state(), blockX, blockY + 1, blockZ) >= kSkyLightMax;
        const std::optional<std::int32_t> here = world.state().blockState(blockX, blockY, blockZ);
        const bool inWater = world::fluids::of(here.value_or(0)) == world::Submersion::Water;
        if (entity::sunBurn::shouldBurn(mob.type(), day, light, openSky, inWater, random) &&
            (!hooks_.combustAllowed || hooks_.combustAllowed(world, mob, entity::sunBurn::kFireTicks, "sunlight"))) {
            mob.fireTicks = entity::sunBurn::kFireTicks;
        }
    }
    if (mob.fireTicks > 0) {
        mob.fireTicks--;
        if (mob.fireTicks % entity::sunBurn::kDamageInterval == 0) {
            const entity::entityCombat::Result result = entity::entityCombat::hurt(
                mob, entity::sunBurn::kFireDamage, std::nullopt, std::nullopt, hooks_.combat, "fire");
            if (result.landed) {
                combat_->announceEntityHit(watchers, mob, world::damageTypes::kOnFire, -1, result.died);
                // parity: `LivingEntity.hurtServer` raises `ENTITY_DAMAGE` at whoever took the hit.
                world::vibrations::emit(world.state(), "entity_damage", mob.x, mob.y, mob.z, world.gameTime());
            }
        }
    }
    const bool burning = mob.fireTicks > 0;
    if (burning == wasBurning) {
        return;
    }
    const auto flags = static_cast<std::int8_t>(burning ? SetEntityDataPacket::kFlagOnFire : 0);
    for (PlayerSession* player : watchers) {
        if (!player->connection().sendPlayPacket) {
            continue;
        }
        player->connection().sendPlayPacket(SetEntityDataPacket::kId, [&](wire::io::ByteBuffer& buffer) {
            SetEntityDataPacket{
                .entityId = mob.entityId(),
                .entries = {SetEntityDataPacket::Entry{.index = SetEntityDataPacket::kSharedFlagsIndex,
                                                       .value = SetEntityDataPacket::ByteValue{.value = flags}}}}
                .write(buffer);
        });
    }
}

void MobActionSystem::tickMobAi(world::LoadedWorld& world, ServerEntity& mob,
                                const std::vector<entity::ai::PlayerView>& views,
                                const std::vector<ServerEntity*>& living, random::RandomSource& random) {
    if (mob.goals == nullptr) {
        mob.goals = entity::ai::mobGoals::forType(mob.type());
        if (mob.goals == nullptr) {
            return;
        }
    }
    entity::ai::MobContext context{.world = world.state(),
                                   .mob = mob,
                                   .random = random,
                                   // The tick's own snapshot, taken once for the whole world: asking for it per mob
                                   // turned a pen of cows into a copy of the entity list per cow per tick.
                                   .entities = living,
                                   .players = views,
                                   .actions = *this,
                                   .light = lightAt(world, entity::mobCombat::blockPosition(mob)),
                                   .night = world::beds::isNight(world.clock().totalTicks()),
                                   .dayTime = world.clock().totalTicks(),
                                   .gameTime = world.gameTime(),
                                   .raidNearby = raids_->raidsIn(world).nearby(mob.x, mob.y, mob.z) != nullptr,
                                   .speed = mob.attributeWithModifiers(entity::attribute::kMovementSpeed)};
    // parity: `Mob.serverAiStep`, which decides on every other tick and staggers the mobs by their
    // own id so they do not all think on the same one.
    if ((mob.tickCount + mob.entityId()) % 2 != 0) {
        mob.goals->tickRunning(context, false);
    } else {
        mob.goals->tick(context);
    }
    // parity: `Mob.travel`'s flying branch: what flies is never walked along a path, and the goals
    // move it themselves, so all that is left here is the drift.
    if (entity::flyers::flies(mob.type())) {
        entity::flyingMovement::tick(world.state(), mob);
        entity::ai::lookControl::tick(mob);
        return;
    }
    entity::ai::navigation::tick(world.state(), mob);
    entity::ai::moveControl::tick(world.state(), mob, context.speed);
    entity::ai::lookControl::tick(mob);
}

void MobActionSystem::activateSkeletonTrap(world::LoadedWorld& world, ServerEntity& horse,
                                           random::RandomSource& random) {
    static_cast<void>(spawning_->activateSkeletonTrap(world, horse, random));
}

void MobActionSystem::tickVillageGolem(world::LoadedWorld& world, ServerEntity& villager,
                                       const std::vector<ServerEntity*>& living, random::RandomSource& random) {
    spawning_->tickVillageGolem(world, villager, living, random);
}

std::int32_t MobActionSystem::lightAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position) {
    const std::int32_t block = world::worldLight::blockLight(world.state(), position[0], position[1], position[2]);
    const std::int32_t sky = world::worldLight::skyLight(world.state(), position[0], position[1], position[2]);
    // parity: `Entity.getLightLevelDependentMagicValue`, whose sky half is darkened by the time of
    // day. Night is taken as darkness rather than tracking the sky brightness itself.
    const std::int32_t daylight = world::beds::isNight(world.clock().totalTicks()) ? 0 : sky;
    return std::max(block, daylight);
}

void MobActionSystem::eatGrassUnder(world::LoadedWorld& world, const ServerEntity& mob) {
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    const auto x = static_cast<std::int32_t>(std::floor(mob.x));
    const auto y = static_cast<std::int32_t>(std::floor(mob.y));
    const auto z = static_cast<std::int32_t>(std::floor(mob.z));
    const std::optional<std::int32_t> here = world.state().blockState(x, y, z);
    const server::registry::BlockState* shortGrass = server::registry::blocks::named("short_grass");
    if (here.has_value() && shortGrass != nullptr && *here == shortGrass->stateId) {
        if (world.state().setBlockState(x, y, z, server::registry::blocks::air().stateId)) {
            broadcaster_->announceBlock(
                watchers,
                world::BlockChange{.x = x, .y = y, .z = z, .stateId = server::registry::blocks::air().stateId});
        }
        return;
    }
    const std::optional<std::int32_t> below = world.state().blockState(x, y - 1, z);
    const server::registry::BlockState* grassBlock = server::registry::blocks::named("grass_block");
    const server::registry::BlockState* dirt = server::registry::blocks::named("dirt");
    if (!below.has_value() || grassBlock == nullptr || dirt == nullptr || *below != grassBlock->stateId) {
        return;
    }
    if (world.state().setBlockState(x, y - 1, z, dirt->stateId)) {
        broadcaster_->announceBlock(watchers, world::BlockChange{.x = x, .y = y - 1, .z = z, .stateId = dirt->stateId});
    }
}

void MobActionSystem::trailSnow(world::LoadedWorld& world, const ServerEntity& golem) {
    if (!world.gameRules().get(world::gameRules::kMobGriefing)) {
        return;
    }
    const server::registry::BlockState* snow = server::registry::blocks::named("snow");
    if (snow == nullptr) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    // parity: `SnowGolem.aiStep` walks four corners a quarter of a block out, which is what leaves a
    // trail two squares wide when the golem walks along a seam.
    for (std::int32_t corner = 0; corner < 4; ++corner) {
        const auto x = static_cast<std::int32_t>(std::floor(golem.x + ((corner % 2 * 2) - 1) * 0.25));
        const auto y = static_cast<std::int32_t>(std::floor(golem.y));
        const auto z = static_cast<std::int32_t>(std::floor(golem.z + ((corner / 2 % 2 * 2) - 1) * 0.25));
        const std::optional<std::int32_t> here = world.state().blockState(x, y, z);
        if (!here.has_value() || *here != server::registry::blocks::air().stateId ||
            !world::blockSurvival::isSupported(world.state(), x, y, z, snow->stateId)) {
            continue;
        }
        if (!world.state().setBlockState(x, y, z, snow->stateId)) {
            continue;
        }
        // parity: `setBlockAndUpdate` is flag 3, so the clients hear about the layer first and the
        // squares around it after, which is the order `Level.setBlock` does the two in.
        broadcaster_->announceBlock(watchers, world::BlockChange{.x = x, .y = y, .z = z, .stateId = snow->stateId});
        broadcaster_->notifyAround(world, watchers, x, y, z, here);
        world::vibrations::emit(world.state(), "block_place", x + 0.5, y + 0.5, z + 0.5, world.gameTime());
    }
}

void MobActionSystem::dropDeathLoot(world::LoadedWorld& world, const ServerEntity& entity) {
    const server::world::loot::LootTable* table = lootTables_->forEntity(entity.type());
    if (table == nullptr) {
        return;
    }
    // parity: an entity's table rolls on the stream it names, `minecraft:entities/<type>`, so the
    // same world drops the same number of chops off the same pig.
    random::RandomSource scratch = freshRandom();
    random::RandomSource& random =
        table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : scratch;
    for (const server::world::loot::ItemStack& stack : table->generate(random)) {
        if (server::registry::items::registry().byName(stack.item) == nullptr) {
            continue;
        }
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = stack.item,
                                                   .count = stack.count,
                                                   .x = entity.x,
                                                   .y = entity.y + (entity.height() / 2.0),
                                                   .z = entity.z});
    }
}

void MobActionSystem::breed(world::LoadedWorld& world, ServerEntity& animal, ServerEntity& partner) {
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(animal.type());
    if (entry == nullptr || (hooks_.breedAllowed && !hooks_.breedAllowed(world, animal, partner))) {
        return;
    }
    auto baby = std::make_unique<ServerEntity>(players_->allocateEntityId(), wire::common::Uuid::random(),
                                               animal.type(), entry->protocolId, animal.x, animal.y, animal.z);
    baby->age = entity::breeding::kBabyAge;
    world.state().spawnEntity(std::move(baby));
    entity::breeding::afterBreeding(animal, partner);
}

} // namespace basalt::server::runtime
