#include <basalt/server/runtime/MobActionSystem.hpp>
#include <basalt/server/runtime/NaturalSpawningSystem.hpp>
#include <basalt/server/runtime/RaidSystem.hpp>
#include <basalt/server/runtime/config/WorldsConfig.hpp>
#include <basalt/server/runtime/entity/Allays.hpp>
#include <basalt/server/runtime/entity/Armadillos.hpp>
#include <basalt/server/runtime/entity/DeathDrops.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/MobConversions.hpp>
#include <basalt/server/runtime/entity/ServerEntity.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/entity/ai/Controls.hpp>
#include <basalt/server/runtime/entity/ai/EndermanGoals.hpp>
#include <basalt/server/runtime/entity/ai/MobContext.hpp>
#include <basalt/server/runtime/entity/ai/SpecialGoals.hpp>
#include <basalt/server/runtime/entity/villager/VillagerBrain.hpp>
#include <basalt/server/runtime/world/LoadedWorld.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <string>

namespace basalt::server::runtime {

using entity::ServerEntity;

std::optional<std::string> MobActionSystem::pluginAmbientSound(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr) {
        return std::nullopt;
    }
    random::RandomSource random = freshRandom();
    return ambientSoundOf(*world, mob, random);
}

std::int32_t MobActionSystem::pluginExperienceReward(ServerEntity& mob) {
    random::RandomSource random = freshRandom();
    return entity::deathDrops::experienceReward(mob, random);
}

bool MobActionSystem::pluginRangedAttack(ServerEntity& mob, double x, double y, double z, float charge) {
    const std::string_view path = mob.type().path();
    if (path == "skeleton" || path == "stray" || path == "bogged" || path == "parched" || path == "wither") {
        shootArrow(mob, x, y, z, charge);
        return true;
    }
    if (path == "llama" || path == "trader_llama") {
        llamaSpit(mob, x, y, z, 0.0);
        return true;
    }
    return false;
}

std::int32_t MobActionSystem::pluginDuplicateAllay(ServerEntity& allay) {
    world::LoadedWorld* world = worldOf(allay);
    if (world == nullptr || spawning_ == nullptr || !entity::allays::isAllay(allay.type())) {
        return -1;
    }
    std::int32_t made = -1;
    static_cast<void>(spawning_->spawnMob(
        *world, allay.type(), allay.x, allay.y, allay.z, allay.yaw, "duplication", [&made](ServerEntity& copy) {
            copy.persistent = true;
            entity::allays::stateOf(copy).duplicationCooldown = entity::allays::kDuplicationCooldown;
            made = copy.entityId();
        }));
    entity::allays::stateOf(allay).duplicationCooldown = entity::allays::kDuplicationCooldown;
    entityEvent(allay, 18);
    return made;
}

bool MobActionSystem::pluginRoll(ServerEntity& armadillo, bool up) {
    world::LoadedWorld* world = worldOf(armadillo);
    if (world == nullptr || armadillo.type().path() != "armadillo") {
        return false;
    }
    entity::armadillos::ArmadilloState& state = entity::armadillos::stateOf(armadillo);
    if (up == (state.state != entity::armadillos::State::Idle)) {
        return false;
    }
    world::vibrations::emit(world->state(), "entity_action", armadillo.x, armadillo.y, armadillo.z, world->gameTime(),
                            {.entity = armadillo.uuid()});
    if (up) {
        entity::ai::navigation::stop(armadillo);
        state.motion.walk.reset();
        armadillo.loveTicks = 0;
        playSound(*world, armadillo, "entity.armadillo.roll", 1.0F, 1.0F, wire::protocol::play::SoundSource::Neutral);
        state.state = entity::armadillos::State::Rolling;
    } else {
        playSound(*world, armadillo, "entity.armadillo.unroll_finish", 1.0F, 1.0F,
                  wire::protocol::play::SoundSource::Neutral);
        state.state = entity::armadillos::State::Idle;
    }
    state.inStateTicks = 0;
    return true;
}

bool MobActionSystem::pluginConverting(ServerEntity& mob) {
    world::LoadedWorld* world = worldOf(mob);
    if (world == nullptr || mob.immuneToZombification || mob.noAi) {
        return false;
    }
    const std::string_view path = mob.type().path();
    if (path != "piglin" && path != "piglin_brute" && path != "hoglin") {
        return false;
    }
    return world->attributeOverride("minecraft:gameplay/piglins_zombify")
               .value_or(world->declaration().dimension != config::WorldDimension::Nether ? 1.0 : 0.0) != 0.0;
}

bool MobActionSystem::pluginCatLying(ServerEntity& cat, bool lying) {
    if (cat.type().path() != "cat") {
        return false;
    }
    cat.lying = lying;
    poseChanged(cat);
    return true;
}

bool MobActionSystem::pluginCatRelaxed(ServerEntity& cat, bool relaxed) {
    if (cat.type().path() != "cat") {
        return false;
    }
    cat.relaxOne = relaxed;
    poseChanged(cat);
    return true;
}

bool MobActionSystem::pluginCelebrate(ServerEntity& raider, bool celebrating) {
    if (!RaidSystem::isRaider(raider)) {
        return false;
    }
    setCelebrating(raider, celebrating);
    return true;
}

bool MobActionSystem::pluginGuardianLaser(ServerEntity& guardian, bool active) {
    const std::string_view path = guardian.type().path();
    if (path != "guardian" && path != "elder_guardian") {
        return false;
    }
    if (!active) {
        setBeamTarget(guardian, std::nullopt);
        return true;
    }
    if (!guardian.targetId.has_value()) {
        return false;
    }
    setBeamTarget(guardian, guardian.targetId);
    return true;
}

bool MobActionSystem::pluginEnderTeleport(ServerEntity& enderman, bool towards, double x, double y, double z) {
    world::LoadedWorld* world = worldOf(enderman);
    if (world == nullptr || enderman.type().path() != "enderman") {
        return false;
    }
    static const std::vector<ServerEntity*> kNobody;
    static const std::vector<entity::ai::PlayerView> kNoPlayers;
    random::RandomSource random = freshRandom();
    entity::ai::MobContext context{.world = world->state(),
                                   .mob = enderman,
                                   .random = random,
                                   .entities = kNobody,
                                   .players = kNoPlayers,
                                   .actions = *this,
                                   .dayTime = world->clock().totalTicks(),
                                   .gameTime = world->gameTime()};
    return towards ? entity::ai::endermanGaze::teleportTowards(context, x, y, z)
                   : entity::ai::teleportEndermanRandomly(context);
}

bool MobActionSystem::pluginRestock(ServerEntity& villager) {
    world::LoadedWorld* world = worldOf(villager);
    if (world == nullptr || villager.type().path() != "villager" || options_.villagerTrades == nullptr) {
        return false;
    }
    std::vector<world::trades::Offer>& offers = entity::villagers::offersOf(*options_.villagerTrades, villager, *world);
    for (world::trades::Offer& offer : offers) {
        offer.demand = offer.demand + offer.uses - (offer.maxUses - offer.uses);
    }
    for (world::trades::Offer& offer : offers) {
        offer.uses = 0;
    }
    entity::villager::VillagerState& state = entity::villager::villagerBrain::stateOf(villager);
    state.lastRestock = world->gameTime();
    ++state.restocksToday;
    return true;
}

bool MobActionSystem::pluginUpdateDemand(ServerEntity& villager) {
    world::LoadedWorld* world = worldOf(villager);
    if (world == nullptr || villager.type().path() != "villager" || options_.villagerTrades == nullptr) {
        return false;
    }
    for (world::trades::Offer& offer : entity::villagers::offersOf(*options_.villagerTrades, villager, *world)) {
        offer.demand = offer.demand + offer.uses - (offer.maxUses - offer.uses);
    }
    return true;
}

std::int32_t MobActionSystem::pluginAddTrades(ServerEntity& villager, std::int32_t amount) {
    world::LoadedWorld* world = worldOf(villager);
    if (world == nullptr || villager.type().path() != "villager" || options_.villagerTrades == nullptr) {
        return 0;
    }
    return entity::villagers::addTrades(*options_.villagerTrades, villager, *world, amount);
}

bool MobActionSystem::pluginResetOffers(ServerEntity& villager) {
    if (villager.type().path() != "villager" && villager.type().path() != "wandering_trader") {
        return false;
    }
    entity::villagers::resetOffers(villager);
    return true;
}

bool MobActionSystem::pluginShakeHead(ServerEntity& villager) {
    world::LoadedWorld* world = worldOf(villager);
    if (world == nullptr || villager.type().path() != "villager") {
        return false;
    }
    constexpr std::int32_t kUnhappyTicks = 40;
    entity::villager::villagerBrain::stateOf(villager).unhappyCounter = kUnhappyTicks;
    entity::villager::VillagerWorld& around = villagersIn(*world);
    around.unhappyChanged(villager, kUnhappyTicks);
    around.sound(villager, "entity.villager.no", 1.0F, 1.0F);
    return true;
}

bool MobActionSystem::pluginSleep(ServerEntity& villager, std::int32_t x, std::int32_t y, std::int32_t z) {
    world::LoadedWorld* world = worldOf(villager);
    if (world == nullptr || villager.type().path() != "villager") {
        return false;
    }
    villagersIn(*world).setSleeping(villager, entity::villager::BlockPos{x, y, z});
    return true;
}

bool MobActionSystem::pluginWake(ServerEntity& villager) {
    world::LoadedWorld* world = worldOf(villager);
    if (world == nullptr || villager.type().path() != "villager") {
        return false;
    }
    villagersIn(*world).setSleeping(villager, std::nullopt);
    return true;
}

std::int32_t MobActionSystem::pluginZombify(ServerEntity& villager) {
    world::LoadedWorld* world = worldOf(villager);
    if (world == nullptr || villager.type().path() != "villager") {
        return -1;
    }
    const wire::nbt::NbtCompound villagerData = entity::entityNbt::encode(villager);
    const entity::mobConversions::Conversion conversion{
        .into = wire::common::Identifier::vanilla("zombie_villager"),
        .finalize = entity::mobConversions::Conversion::Finalize::ZombieVillager};
    std::int32_t spawnedId = -1;
    const bool converted = convertMob(
        *world, watchersOf(*world), villager, conversion,
        [&villagerData](wire::nbt::NbtCompound& tag) {
            for (const std::string_view key : {"VillagerData", "Offers", "Gossips", "Xp"}) {
                if (const wire::nbt::NbtTag* value = villagerData.find(key)) {
                    tag.set(key, *value);
                }
            }
            tag.putBoolean("VillagerDataFinalized", true);
        },
        &spawnedId);
    return converted ? spawnedId : -1;
}

} // namespace basalt::server::runtime
