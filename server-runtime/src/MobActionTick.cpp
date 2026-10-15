#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BlockChangeBroadcaster.hpp>
#include <basalt/server/runtime/CombatSystem.hpp>
#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/config/GameplayRules.hpp>
#include <basalt/server/runtime/config/MobsConfig.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Attributes.hpp>
#include <basalt/server/runtime/entity/Bees.hpp>
#include <basalt/server/runtime/entity/Breeding.hpp>
#include <basalt/server/runtime/entity/Camels.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/FlyingMovement.hpp>
#include <basalt/server/runtime/entity/Frogs.hpp>
#include <basalt/server/runtime/entity/GeneratedEntityPresentation.hpp>
#include <basalt/server/runtime/entity/Horses.hpp>
#include <basalt/server/runtime/entity/LivingMovement.hpp>
#include <basalt/server/runtime/entity/MobBreathing.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/MobMisc.hpp>
#include <basalt/server/runtime/entity/MobOverrides.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/Piglins.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/GoalSelector.hpp>
#include <basalt/server/runtime/entity/ai/MobGoals.hpp>
#include <basalt/server/runtime/player/PlayerRegistry.hpp>
#include <basalt/server/runtime/player/PlayerSession.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/Beds.hpp>
#include <basalt/server/runtime/world/BlockSurvival.hpp>
#include <basalt/server/runtime/world/BreedingCooldowns.hpp>
#include <basalt/server/runtime/world/DroppedItem.hpp>
#include <basalt/server/runtime/world/FluidFlow.hpp>
#include <basalt/server/runtime/world/Fluids.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Precipitation.hpp>
#include <basalt/server/runtime/world/RealDamageTypeTags.hpp>
#include <basalt/server/runtime/world/RealLootTables.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>
#include <basalt/wire/common/Mth.hpp>
#include <basalt/wire/protocol/play/EntityData.hpp>
#include <basalt/wire/protocol/play/EntityPackets.hpp>

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
    const bool sensitive = mob.burnsInDay >= 0 ? mob.burnsInDay != 0 : entity::sunBurn::burns(mob.type());
    if (sensitive) {
        const bool day = !world::beds::isNight(world.clock().totalTicks());
        const std::int32_t light = world::worldLight::skyLight(world.state(), blockX, blockY, blockZ);
        const bool openSky = world::worldLight::skyLight(world.state(), blockX, blockY + 1, blockZ) >= kSkyLightMax;
        const std::optional<std::int32_t> here = world.state().blockState(blockX, blockY, blockZ);
        const bool inWater = world::fluids::of(here.value_or(0)) == world::Submersion::Water;
        if (entity::sunBurn::shouldBurn(sensitive, day, light, openSky, inWater, random)) {
            std::int32_t fireTicks = entity::sunBurn::kFireTicks;
            if (!hooks_.combustAllowed || hooks_.combustAllowed(world, mob, fireTicks, "sunlight")) {
                mob.fireTicks = fireTicks;
            }
        }
    }
    // The enderman's own teleport goal already hurts it in water.
    if (mob.tuning != nullptr && mob.tuning->takesDamageFromWater == true && mob.type().path() != "enderman") {
        const std::optional<std::int32_t> feet = world.state().blockState(blockX, blockY, blockZ);
        if (world::fluids::of(feet.value_or(0)) == world::Submersion::Water ||
            world::precipitation::entityInRain(world, mob.x, mob.y, mob.z, mob.height())) {
            const entity::entityCombat::Result result =
                entity::entityCombat::hurt(mob, 1.0F, std::nullopt, std::nullopt, hooks_.combat, "drown");
            if (result.landed) {
                combat_->announceEntityHit(watchers, mob, world::damageTypes::kDrown, -1, result.died);
            }
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
                world::vibrations::emit(world.state(), "entity_damage", mob.x, mob.y, mob.z, world.gameTime(),
                                        {.entity = mob.uuid()});
            }
        }
    }
    const bool burning = mob.fireTicks > 0;
    if (burning == wasBurning) {
        return;
    }
    const std::int8_t flags = entity::sharedFlagsOf(mob);
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

void MobActionSystem::tickBreath(world::LoadedWorld& world, const std::vector<PlayerSession*>& watchers,
                                 ServerEntity& mob, random::RandomSource& random) {
    constexpr std::int32_t kAirIndex = 1;
    constexpr std::int32_t kMoistnessIndex = 19;
    constexpr std::int32_t kFullMoistness = 2400;
    const std::int32_t air = mob.airSupply;
    const std::int32_t moistness = mob.moistness;
    const auto hurt = [&](float amount, std::string_view cause) {
        const entity::entityCombat::Result result =
            entity::entityCombat::hurt(mob, amount, std::nullopt, std::nullopt, hooks_.combat, cause);
        if (result.landed) {
            combat_->announceEntityHit(watchers, mob,
                                       cause == "drown" ? world::damageTypes::kDrown : world::damageTypes::kDryOut, -1,
                                       result.died);
        }
    };
    const auto stateAt = [&world](double x, double y, double z) {
        return world.state()
            .blockState(wire::common::mth::floor(x), wire::common::mth::floor(y), wire::common::mth::floor(z))
            .value_or(0);
    };
    const auto isWater = [](std::int32_t state) { return world::fluidFlow::kindOf(state) == world::FluidKind::Water; };
    // parity: `isEyeInFluid`.
    const double eyeY = mob.y + entity::mobBreathing::eyeHeightOf(mob);
    const std::int32_t eyeBlockY = wire::common::mth::floor(eyeY);
    const std::int32_t eyeState = stateAt(mob.x, eyeY, mob.z);
    const wire::common::Identifier* eyeBlock = registry::blockStateDefinitions::blockOf(eyeState);
    const bool eyeInWater = isWater(eyeState) && eyeY < eyeBlockY + static_cast<double>(world::fluidFlow::heightAt(
                                                                        world.state(), wire::common::mth::floor(mob.x),
                                                                        eyeBlockY, wire::common::mth::floor(mob.z)));
    const bool inWater = isWater(stateAt(mob.x, mob.y, mob.z));
    const bool inRain = world::precipitation::entityInRain(world, mob.x, mob.y, mob.z, mob.height());
    const entity::mobBreathing::Surroundings at{.eyeInWater = eyeInWater,
                                                .eyeInBubbles =
                                                    eyeBlock != nullptr && eyeBlock->path() == "bubble_column",
                                                .inWater = inWater,
                                                .inRain = inRain,
                                                .oxygenBonus = mob.attributeWithModifiers("minecraft:oxygen_bonus"),
                                                .random = &random};
    for (const entity::mobBreathing::Hurt& owed : entity::mobBreathing::tick(mob, at)) {
        if (owed.drownParticles) {
            // parity: `EntityEvent.DROWN_PARTICLES`, broadcast before the hit.
            const wire::protocol::play::EntityEventPacket event{.entityId = mob.entityId(), .eventId = 67};
            for (PlayerSession* player : watchers) {
                if (player->connection().sendPlayPacket) {
                    player->connection().sendPlayPacket(
                        wire::protocol::play::EntityEventPacket::kId,
                        [&event](wire::io::ByteBuffer& buffer) { event.write(buffer); });
                }
            }
        }
        hurt(owed.amount, owed.cause);
    }
    // parity: `Dolphin.tick`: two minutes of moisture, and the flop on dry ground.
    if (mob.type().path() == "dolphin" && !mob.noAi) {
        if (inWater || inRain) {
            mob.moistness = kFullMoistness;
        } else {
            mob.moistness--;
            if (mob.moistness <= 0) {
                hurt(1.0F, "dry_out");
            }
            if (mob.onGround) {
                mob.velocityX += static_cast<double>((random.nextFloat() * 2.0F - 1.0F) * 0.2F);
                mob.velocityY += 0.5;
                mob.velocityZ += static_cast<double>((random.nextFloat() * 2.0F - 1.0F) * 0.2F);
                mob.yaw = random.nextFloat() * 360.0F;
                mob.onGround = false;
            }
        }
    }
    std::vector<SetEntityDataPacket::Entry> entries;
    if (mob.airSupply != air) {
        entries.push_back({.index = kAirIndex, .value = SetEntityDataPacket::IntValue{.value = mob.airSupply}});
    }
    if (mob.moistness != moistness) {
        entries.push_back({.index = kMoistnessIndex, .value = SetEntityDataPacket::IntValue{.value = mob.moistness}});
    }
    if (entries.empty()) {
        return;
    }
    const SetEntityDataPacket packet{.entityId = mob.entityId(), .entries = std::move(entries)};
    for (PlayerSession* player : watchers) {
        if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(SetEntityDataPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

/// parity: `LivingEntity.aiStep`.
void MobActionSystem::travelMob(world::LoadedWorld& world, ServerEntity& mob, double speed,
                                random::RandomSource* random, const std::optional<std::array<double, 3>>& target) {
    // parity: `FlyingPathNavigation`.
    if (entity::flyers::flies(mob.type())) {
        // parity: a bee's navigation holds still while it pollinates.
        if (entity::flyingMovement::usesFlyingPath(mob.type()) && (mob.bee == nullptr || !mob.bee->pollinating)) {
            entity::ai::navigation::tick(world.state(), mob);
        }
        random::RandomSource scratch = freshRandom();
        entity::flyingMovement::tick(world.state(), mob, random != nullptr ? *random : scratch, target);
        entity::ai::lookControl::tick(mob);
        return;
    }
    // parity: `Blaze.aiStep`, which falls at six tenths of the speed.
    if (mob.type().path() == "blaze" && !mob.onGround && mob.velocityY < 0.0) {
        mob.velocityY *= 0.6;
    }
    entity::ai::navigation::tick(world.state(), mob);
    // parity: `CreakingMoveControl` and `CreakingLookControl` do nothing while it is frozen.
    if (mob.creaking != nullptr && !mob.creaking->canMove) {
        entity::ai::moveControl::stop(mob);
        entity::ai::moveControl::tick(world.state(), mob, speed);
        return;
    }
    entity::ai::moveControl::tick(world.state(), mob, speed);
    entity::ai::lookControl::tick(mob);
}

void MobActionSystem::tickRidden(world::LoadedWorld& world, ServerEntity& mob, const PlayerSession& rider) {
    constexpr std::int32_t kJumpInput = 0x10;
    // parity: `LivingEntity.tickRidden` turns the mount with its rider and the pitch to half.
    mob.yaw = rider.yaw;
    mob.headYaw = rider.yaw;
    mob.pitch = rider.pitch * 0.5F;
    entity::ai::navigation::stop(mob);
    // parity: `getRiddenInput`: strafing at half strength and backing up at a quarter.
    const double strafe = ((rider.inputLeft ? 1.0 : 0.0) - (rider.inputRight ? 1.0 : 0.0)) * 0.5;
    double forward = (rider.inputForward ? 1.0 : 0.0) - (rider.inputBack ? 1.0 : 0.0);
    if (forward <= 0.0) {
        forward *= 0.25;
    }
    if ((rider.extras.inputFlags & kJumpInput) != 0) {
        entity::livingMovement::jump(world.state(), mob);
    }
    entity::ai::moveControl::strafe(mob, strafe, forward, 1.0);
    entity::ai::moveControl::tick(world.state(), mob, mob.attributeWithModifiers(entity::attribute::kMovementSpeed));
}

namespace {

using BrainTick = void (MobActionSystem::*)(world::LoadedWorld&, entity::ai::MobContext&);

[[nodiscard]] BrainTick brainOf(const wire::common::Identifier& type) {
    const std::string_view path = type.path();
    if (entity::allays::isAllay(type)) {
        return &MobActionSystem::tickAllay;
    }
    if (entity::piglins::isPiglin(type)) {
        return &MobActionSystem::tickPiglin;
    }
    if (path == "hoglin") {
        return &MobActionSystem::tickHoglin;
    }
    if (path == "axolotl") {
        return &MobActionSystem::tickAxolotl;
    }
    if (path == "goat") {
        return &MobActionSystem::tickGoat;
    }
    if (entity::camels::isCamel(type)) {
        return &MobActionSystem::tickCamel;
    }
    if (path == "breeze") {
        return &MobActionSystem::tickBreeze;
    }
    if (path == "armadillo") {
        return &MobActionSystem::tickArmadillo;
    }
    if (path == "sniffer") {
        return &MobActionSystem::tickSniffer;
    }
    if (path == "frog") {
        return &MobActionSystem::tickFrog;
    }
    if (path == "tadpole") {
        return &MobActionSystem::tickTadpole;
    }
    if (path == "creaking") {
        return &MobActionSystem::tickCreaking;
    }
    return nullptr;
}

} // namespace

void MobActionSystem::retarget(world::LoadedWorld& world, ServerEntity& mob,
                               const std::optional<std::int32_t>& previous) {
    if (!hooks_.targetChanged) {
        return;
    }
    std::optional<std::int32_t> next = mob.targetId;
    mob.targetId = hooks_.targetChanged(world, mob, previous, next) ? next : previous;
    if (mob.targetId.has_value() && mob.targetId != previous) {
        const std::int32_t id = *mob.targetId;
        const ServerEntity* found = world.state().entity(id);
        const std::vector<PlayerSession*> watchers = watchersOf(world);
        const bool player =
            std::ranges::any_of(watchers, [id](const PlayerSession* each) { return each->entityId() == id; });
        if ((found == nullptr || found->removed) && !player) {
            mob.targetId = previous;
        }
    }
}

void MobActionSystem::tickMobAi(world::LoadedWorld& world, ServerEntity& mob,
                                const std::vector<entity::ai::PlayerView>& views,
                                const std::vector<ServerEntity*>& living, random::RandomSource& random) {
    if (mob.type().path() == "villager") {
        tickVillager(world, mob, views, living, random);
        return;
    }
    if (mob.goals == nullptr) {
        mob.goals = entity::ai::mobGoals::forType(mob.type());
        if (mob.goals != nullptr && mob.tuning != nullptr && mob.tuning->disabledGoals.has_value()) {
            for (const std::string& goal : *mob.tuning->disabledGoals) {
                mob.goals->remove(goal);
            }
        }
    }
    // parity: `Mob.serverAiStep`.
    // parity: `Raider.aiStep`.
    const bool raider = RaidSystem::isRaider(mob);
    if (raider) {
        raids_->tickRaider(world, mob);
    }
    if (mob.type().path() == "witch") {
        tickWitchDrinking(world, mob, random);
    }
    if (mob.type().path() == "strider") {
        tickStriderWarmth(world, mob);
    }
    if (mob.misc != nullptr && mob.misc->boosting && mob.misc->boostTime++ > mob.misc->boostTotal) {
        mob.misc->boosting = false;
    }
    const BrainTick brain = brainOf(mob.type());
    if (mob.goals == nullptr && brain == nullptr) {
        travelMob(world, mob, mob.attributeWithModifiers(entity::attribute::kMovementSpeed), &random);
        return;
    }
    entity::ai::MobContext context{.world = world.state(),
                                   .mob = mob,
                                   .random = random,
                                   // One snapshot per tick for the whole world: per mob it copied the entity list.
                                   .entities = living,
                                   .players = views,
                                   .actions = *this,
                                   .light = lightAt(world, entity::mobCombat::blockPosition(mob)),
                                   .night = world::beds::isNight(world.clock().totalTicks()),
                                   .dayTime = world.clock().totalTicks(),
                                   .gameTime = world.gameTime(),
                                   .universalAnger = world.gameRules().get(world::gameRules::kUniversalAnger),
                                   .raid = raider ? raids_->raidOf(world, mob) : std::nullopt,
                                   .speed = mob.attributeWithModifiers(entity::attribute::kMovementSpeed),
                                   .skyDarken = world.skyDarken(),
                                   .hasSkylight = config::dimensionFacts(world.declaration().dimension).hasSkylight,
                                   .ambientLight = config::dimensionFacts(world.declaration().dimension).ambientLight,
                                   .seaLevel = config::dimensionFacts(world.declaration().dimension).seaLevel,
                                   .minY = config::dimensionFacts(world.declaration().dimension).chunkHeight.minY()};
    // parity: `Allay.customServerAiStep`.
    const bool goalsToo =
        mob.goals != nullptr && (mob.type().path() == "tadpole" || (brain != nullptr && mob.goals->hasPluginGoals()));
    if (brain != nullptr && !goalsToo) {
        (this->*brain)(world, context);
        travelMob(world, mob, context.speed, &random);
        return;
    }
    // parity: `Mob.serverAiStep`.
    if ((mob.tickCount + mob.entityId()) % 2 != 0) {
        mob.goals->tickRunning(context, false);
    } else {
        mob.goals->tick(context);
    }
    if (goalsToo) {
        (this->*brain)(world, context);
    }
    // parity: the target a vex faces and a blaze rises to, as it stands after the goals.
    std::optional<std::array<double, 3>> target;
    double targetEye = 0.0;
    if (mob.targetId.has_value()) {
        if (const entity::ai::PlayerView* player = context.playerById(*mob.targetId); player != nullptr) {
            target = std::array<double, 3>{player->x, player->y, player->z};
            targetEye = player->y + static_cast<double>(1.62F);
        } else if (const ServerEntity* other = context.entityById(*mob.targetId); other != nullptr && !other->removed) {
            target = std::array<double, 3>{other->x, other->y, other->z};
            targetEye = other->y + entity::mobBreathing::eyeHeightOf(*other);
        }
    }
    if (mob.type().path() == "blaze") {
        blazeRises(mob, target.has_value() ? std::optional<double>{targetEye} : std::nullopt, random);
    }
    travelMob(world, mob, context.speed, &random, target);
}

void MobActionSystem::tickVexLife(const std::vector<PlayerSession*>& watchers, ServerEntity& vex) {
    // parity: `Vex.tick`.
    if (vex.flight.limitedLifeTicks < 0 && vex.sourceNbt.has_value() && vex.sourceNbt->contains("life_ticks")) {
        vex.flight.limitedLifeTicks = vex.sourceNbt->getInt("life_ticks");
    }
    if (vex.flight.limitedLifeTicks < 0) {
        return;
    }
    const bool due = --vex.flight.limitedLifeTicks <= 0;
    if (!vex.sourceNbt.has_value()) {
        vex.sourceNbt.emplace();
    }
    vex.sourceNbt->putInt("life_ticks", due ? 20 : vex.flight.limitedLifeTicks);
    if (!due) {
        return;
    }
    vex.flight.limitedLifeTicks = 20;
    const entity::entityCombat::Result result =
        entity::entityCombat::hurt(vex, 1.0F, std::nullopt, std::nullopt, hooks_.combat, "starve");
    if (result.landed) {
        combat_->announceEntityHit(watchers, vex, world::damageTypes::kStarve, -1, result.died);
    }
}

/// parity: `Blaze.customServerAiStep`.
void MobActionSystem::blazeRises(ServerEntity& blaze, std::optional<double> targetEye, random::RandomSource& random) {
    ServerEntity::Flight& flight = blaze.flight;
    if (--flight.blazeHeightOffsetTicks <= 0) {
        flight.blazeHeightOffsetTicks = 100;
        flight.blazeHeightOffset = static_cast<float>(random.triangle(0.5, 6.891));
    }
    if (targetEye.has_value() && *targetEye > blaze.y + entity::mobBreathing::eyeHeightOf(blaze) +
                                                  static_cast<double>(flight.blazeHeightOffset)) {
        blaze.velocityY += (static_cast<double>(0.3F) - blaze.velocityY) * static_cast<double>(0.3F);
    }
}

void MobActionSystem::activateSkeletonTrap(world::LoadedWorld& world, ServerEntity& horse,
                                           random::RandomSource& random) {
    static_cast<void>(spawning_->activateSkeletonTrap(world, horse, random));
}

std::int32_t MobActionSystem::lightAt(world::LoadedWorld& world, const std::array<std::int32_t, 3>& position) {
    const std::int32_t block = world::worldLight::blockLight(world.state(), position[0], position[1], position[2]);
    const std::int32_t sky = world::worldLight::skyLight(world.state(), position[0], position[1], position[2]);
    // parity: `Entity.getLightLevelDependentMagicValue`.
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
        if (plugins::hookEvents::changeBlockAllowed(world.events(), world.name(), mob, x, y, z, *here,
                                                    server::registry::blocks::air().stateId, "eat") &&
            world.state().setBlockState(x, y, z, server::registry::blocks::air().stateId)) {
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
    if (plugins::hookEvents::changeBlockAllowed(world.events(), world.name(), mob, x, y - 1, z, *below, dirt->stateId,
                                                "eat") &&
        world.state().setBlockState(x, y - 1, z, dirt->stateId)) {
        broadcaster_->announceBlock(watchers, world::BlockChange{.x = x, .y = y - 1, .z = z, .stateId = dirt->stateId});
    }
}

void MobActionSystem::trailSnow(world::LoadedWorld& world, const ServerEntity& golem) {
    if (!entity::mobOverrides::griefing(golem, world.gameRules().get(world::gameRules::kMobGriefing))) {
        return;
    }
    const server::registry::BlockState* snow = server::registry::blocks::named("snow");
    if (snow == nullptr) {
        return;
    }
    const std::vector<PlayerSession*> watchers = watchersOf(world);
    // parity: `SnowGolem.aiStep`.
    for (std::int32_t corner = 0; corner < 4; ++corner) {
        const auto x = static_cast<std::int32_t>(std::floor(golem.x + ((corner % 2 * 2) - 1) * 0.25));
        const auto y = static_cast<std::int32_t>(std::floor(golem.y));
        const auto z = static_cast<std::int32_t>(std::floor(golem.z + ((corner / 2 % 2 * 2) - 1) * 0.25));
        const std::optional<std::int32_t> here = world.state().blockState(x, y, z);
        if (!here.has_value() || *here != server::registry::blocks::air().stateId ||
            !world::blockSurvival::isSupported(world.state(), x, y, z, snow->stateId)) {
            continue;
        }
        if (!plugins::hookEvents::changeBlockAllowed(world.events(), world.name(), golem, x, y, z, *here, snow->stateId,
                                                     "trail") ||
            !world.state().setBlockState(x, y, z, snow->stateId)) {
            continue;
        }
        // parity: `setBlockAndUpdate`.
        broadcaster_->announceBlock(watchers, world::BlockChange{.x = x, .y = y, .z = z, .stateId = snow->stateId});
        broadcaster_->notifyAround(world, watchers, x, y, z, here);
        world::vibrations::emit(world.state(), "block_place", x + 0.5, y + 0.5, z + 0.5, world.gameTime(),
                                {.entity = golem.uuid()});
    }
}

void MobActionSystem::dropDeathLoot(world::LoadedWorld& world, const ServerEntity& entity) {
    dropDeathInventories(world, entity);
    spawnLoot(world, entity, deathLootOf(world, entity));
}

void MobActionSystem::dropDeathInventories(world::LoadedWorld& world, const ServerEntity& entity) {
    if (entity::allays::isAllay(entity.type())) {
        dropAllayInventory(world, entity);
    }
    if (entity::piglins::isPiglin(entity.type())) {
        dropPiglinInventory(world, entity);
    }
}

std::vector<server::world::loot::ItemStack> MobActionSystem::deathLootOf(world::LoadedWorld& world,
                                                                         const ServerEntity& entity) {
    // parity: `Mob.getLootTable`, a `DeathLootTable` in the tag wins over `minecraft:entities/<type>`.
    const server::world::loot::LootTable* table = nullptr;
    if (entity.deathLootTable.has_value()) {
        if (const std::expected<wire::common::Identifier, wire::common::IdentifierError> named =
                wire::common::Identifier::parse(*entity.deathLootTable);
            named.has_value()) {
            table = lootTables_->byId(*named);
        }
    } else {
        table = lootTables_->forEntity(entity.type());
    }
    if (table == nullptr) {
        return {};
    }
    // parity: `LootTable.getRandomItems(params, seed)`, a nonzero seed gives the table its own random.
    random::RandomSource scratch =
        entity.deathLootTableSeed != 0 ? random::RandomSource::legacy(entity.deathLootTableSeed) : freshRandom();
    random::RandomSource& random =
        table->randomSequence.has_value() ? world.randomSequences().of(*table->randomSequence) : scratch;
    const server::world::loot::LootContext context = deathLootContext(world, entity);
    return table->generate(random, context.luck, std::nullopt, context);
}

void MobActionSystem::spawnLoot(world::LoadedWorld& world, const ServerEntity& entity,
                                const std::vector<server::world::loot::ItemStack>& stacks) {
    for (const server::world::loot::ItemStack& stack : stacks) {
        if (stack.count <= 0 || server::registry::items::registry().byName(stack.item) == nullptr) {
            continue;
        }
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = stack.item,
                                                   .count = stack.count,
                                                   .x = entity.x,
                                                   .y = entity.y + (entity.height() / 2.0),
                                                   .z = entity.z,
                                                   .components = stack.components});
    }
}

void MobActionSystem::askDeathDrops(world::LoadedWorld& world, const ServerEntity& entity,
                                    std::vector<server::world::loot::ItemStack>& drops, std::int32_t& experience) {
    if (hooks_.deathDrops) {
        hooks_.deathDrops(world, entity, drops, experience);
    }
}

void MobActionSystem::breed(world::LoadedWorld& world, ServerEntity& animal, ServerEntity& partner) {
    const server::registry::EntityTypeEntry* entry = server::registry::entityTypes::of(animal.type());
    if (entry == nullptr) {
        return;
    }
    // parity: `Animal.spawnChildFromBreeding` rolls the orb before `EntityBreedEvent`, whose cancel resets the love.
    std::int32_t experience = freshRandom().nextInt(7) + 1;
    const std::optional<std::string>& breeder = animal.loveCause.has_value() ? animal.loveCause : partner.loveCause;
    if (hooks_.breedAllowed &&
        !hooks_.breedAllowed(world, animal, partner, breeder.value_or(std::string{}), experience)) {
        for (ServerEntity* parent : {&animal, &partner}) {
            parent->loveTicks = 0;
            parent->loveCause.reset();
            parent->courtshipTicks = 0;
        }
        return;
    }
    // parity: `Frog.spawnChildFromBreeding`.
    const bool frog = animal.type().path() == "frog";
    const wire::common::Identifier childType =
        entity::horses::isHorse(animal.type()) ? entity::horses::offspringType(animal, partner) : animal.type();
    const bool sniffer = animal.type().path() == "sniffer";
    if (frog) {
        entity::frogs::stateOf(animal).pregnant = true;
    } else if (animal.type().path() == "turtle") {
        // parity: `TurtleBreedGoal.breed`, an egg to lay instead of a calf.
        entity::mobMisc::of(animal).turtleHasEgg = true;
    } else if (sniffer) {
        // parity: `Sniffer.spawnChildFromBreeding`: an egg dropped where it stands, with a plop.
        random::RandomSource random = freshRandom();
        world.state().spawnItem(world::DroppedItem{.entityId = players_->allocateEntityId(),
                                                   .uuid = wire::common::Uuid::random(),
                                                   .item = wire::common::Identifier::vanilla("sniffer_egg"),
                                                   .count = 1,
                                                   .x = animal.x,
                                                   .y = animal.y,
                                                   .z = animal.z,
                                                   .velocityX = (random.nextDouble() * 0.2) - 0.1,
                                                   .velocityY = 0.2,
                                                   .velocityZ = (random.nextDouble() * 0.2) - 0.1,
                                                   .pickupDelay = world::kSpawnedPickupDelay});
        playSound(world, animal, "entity.sniffer.egg_plop", 1.0F,
                  ((random.nextFloat() - random.nextFloat()) * 0.2F) + 0.5F,
                  wire::protocol::play::SoundSource::Neutral);
    } else {
        const server::registry::EntityTypeEntry* childEntry = server::registry::entityTypes::of(childType);
        std::int32_t offspring = 1;
        if (const config::MobOverride* tweak = animal.tuning.get(); tweak != nullptr) {
            random::RandomSource random = freshRandom();
            const std::int32_t least = tweak->offspringMin.value_or(1);
            const std::int32_t most = std::max(least, tweak->offspringMax.value_or(least));
            offspring = random.nextDouble() < tweak->offspringChance.value_or(1.0)
                            ? least + (most > least ? random.nextInt(most - least + 1) : 0)
                            : 0;
        }
        for (std::int32_t each = 0; each < offspring; ++each) {
            auto baby = std::make_unique<ServerEntity>(
                players_->allocateEntityId(), wire::common::Uuid::random(), childType,
                childEntry == nullptr ? entry->protocolId : childEntry->protocolId, animal.x, animal.y, animal.z);
            baby->age = entity::breeding::kBabyAge;
            if (entity::horses::isHorse(animal.type())) {
                random::RandomSource random = freshRandom();
                entity::horses::inherit(*baby, animal, partner, random);
            }
            world.state().spawnEntity(std::move(baby));
        }
    }
    // parity: `finalizeSpawnChildFromBreeding` credits whoever fed either parent.
    if (breeder.has_value() && hooks_.bredAnimals) {
        hooks_.bredAnimals(*breeder, animal.type(), partner.type(), childType);
    }
    if (breeder.has_value()) {
        world::breedingCooldowns::record(*breeder, animal.type(), world.gameTime(),
                                         world.state().gameplay().mechanics.animalBreedingCooldownSeconds);
    }
    entity::breeding::afterBreeding(animal, partner);
    // parity: `Animal.finalizeSpawnChildFromBreeding`, one orb of one to seven points.
    if (world.gameRules().get(world::gameRules::kMobDrops) && hooks_.spawnOrb && experience > 0) {
        hooks_.spawnOrb(world, animal.x, animal.y, animal.z, experience);
    }
}

} // namespace basalt::server::runtime
