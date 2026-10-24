#include "BasaltServerTickShared.hpp"

#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/BlockTests.hpp>
#include <basalt/server/runtime/FishingSystem.hpp>
#include <basalt/server/runtime/LightningBoltSystem.hpp>
#include <basalt/server/runtime/entity/AreaEffectClouds.hpp>
#include <basalt/server/runtime/entity/Boats.hpp>
#include <basalt/server/runtime/entity/Creakings.hpp>
#include <basalt/server/runtime/entity/DeathDrops.hpp>
#include <basalt/server/runtime/entity/EnderDragon.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/FireworkRockets.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/PrimedTnt.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/entity/Shulkers.hpp>
#include <basalt/server/runtime/entity/SwimPath.hpp>
#include <basalt/server/runtime/entity/Villagers.hpp>
#include <basalt/server/runtime/player/PlayerSounds.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/AttachedEntities.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/ContainerDrops.hpp>
#include <basalt/server/runtime/world/ContainerLoot.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/EndCrystals.hpp>
#include <basalt/server/runtime/world/EnderEyes.hpp>
#include <basalt/server/runtime/world/Fires.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/GameRules.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>
#include <basalt/server/runtime/world/ItemFrames.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/RealBlockTags.hpp>
#include <basalt/server/runtime/world/RealItemTags.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/TriggerFacts.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/storage/StoredPlayer.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <random>
#include <set>
#include <string>

namespace basalt::server::runtime {
using namespace basaltServerTick;

void BasaltServer::tickBoat(world::LoadedWorld& world, const std::vector<player::PlayerSession*>& watchers,
                            const std::vector<entity::ServerEntity*>& /*living*/, entity::ServerEntity& boat) {
    player::PlayerSession* driver = nullptr;
    for (player::PlayerSession* player : watchers) {
        if (!boat.passengerIds.empty() && player->entityId() == boat.passengerIds.front()) {
            driver = player;
        }
    }
    // parity: `AbstractBoat.tick` starts with `super.tick()`.
    pushes_->pushByFluid(world, boat);
    const entity::boats::TickResult result = entity::boats::tick(world.state(), boat, driver != nullptr);
    // parity: `applyEffectsFromBlocks` and `tickBubbleColumn`.
    vehicles_->tickBoatBubbles(world, boat, driver != nullptr);
    // parity: `level.playSound(null, ...)` for each stroke.
    for (const entity::boats::TickResult::Stroke& stroke : result.strokes) {
        random::RandomSource random = random::RandomSource::legacy(scratchSeed());
        const float pitch = 0.8F + (0.4F * random.nextFloat());
        for (player::PlayerSession* player : watchers) {
            player::playSoundFor(*player, stroke.sound, wire::protocol::play::SoundSource::Neutral, stroke.x, stroke.y,
                                 stroke.z, 1.0F, pitch, random.nextLong());
        }
    }
    bool changed = false;
    if (result.ejectAll) {
        // parity: `ejectPassengers`, the last one first.
        const std::vector<std::int32_t> aboard = boat.passengerIds;
        for (auto id = aboard.rbegin(); id != aboard.rend(); ++id) {
            bool wasPlayer = false;
            for (player::PlayerSession* player : watchers) {
                if (player->entityId() == *id && player->connection().dismount) {
                    player->connection().dismount();
                    wasPlayer = true;
                }
            }
            if (!wasPlayer) {
                if (entity::ServerEntity* mob = world.state().entity(*id); mob != nullptr) {
                    mob->vehicleId.reset();
                    mob->boardingCooldown = 60;
                }
                std::erase(boat.passengerIds, *id);
                changed = true;
            }
        }
    }
    vehicles_->boatPushes(world, boat, driver != nullptr);
    if (!changed) {
        return;
    }
    const wire::protocol::play::SetPassengersPacket packet{.vehicleId = boat.entityId(),
                                                           .passengerIds = boat.passengerIds};
    for (player::PlayerSession* player : watchers) {
        if (player->connection().sendPassengers) {
            player->connection().sendPassengers(boat.entityId(), boat.passengerIds);
        } else if (player->connection().sendPlayPacket) {
            player->connection().sendPlayPacket(wire::protocol::play::SetPassengersPacket::kId,
                                                [&packet](wire::io::ByteBuffer& buffer) { packet.write(buffer); });
        }
    }
}

bool BasaltServer::drinkBySculk(world::LoadedWorld& world, const entity::ServerEntity& dead) {
    // parity: `ENTITY_DIE`.
    const std::optional<std::array<std::int32_t, 3>> catalyst =
        sculkCatalysts_ == nullptr ? std::nullopt : sculkCatalysts_->nearest(world, dead.x, dead.y, dead.z);
    if (!catalyst.has_value()) {
        return false;
    }
    random::RandomSource random = random::RandomSource::legacy(scratchSeed());
    const std::int32_t reward =
        entity::deathDrops::shouldDropExperience(dead) ? entity::deathDrops::experienceReward(dead, random) : 0;
    sculkCatalysts_->drink(world, *catalyst, dead.x, dead.y, dead.z, reward > 0 ? std::optional{reward} : std::nullopt);
    // parity: `tryAwardItSpreadsAdvancement`, for a player that was the last to hurt it.
    if (reward > 0 && dead.hurtBy.has_value() && advancementTriggers_ != nullptr) {
        for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
            if (player->entityId() == *dead.hurtBy && player->worldName == world.name()) {
                advancementTriggers_->fire(
                    "kill_mob_near_sculk_catalyst", player->name(), player->uuid().toString(),
                    world::TriggerEvent{.entities = {{"entity", world::triggerFacts::ofEntity(world, dead)}},
                                        .damage = killingBlowOf(world, dead)});
            }
        }
    }
    return true;
}

void BasaltServer::tickDeaths(world::LoadedWorld& world, const sched::Region& region) {
    for (entity::ServerEntity* dead : world.state().entities()) {
        if (!dead->killed || !region.ownsPosition(dead->x, dead->z)) {
            continue;
        }
        dead->killed = false;
        // parity: `source.getEntity()` of the blow that killed it, a player or a mob.
        std::optional<wire::common::Uuid> murderer;
        bool murdererIsPlayer = false;
        if (const std::optional<std::int32_t> causing = dead->lastBlame.causingId; causing.has_value()) {
            if (const entity::ServerEntity* mob = world.state().entity(*causing); mob != nullptr) {
                murderer = mob->uuid();
            }
            for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
                if (player->entityId() == *causing) {
                    murderer = player->uuid();
                    murdererIsPlayer = true;
                }
            }
        }
        // parity: `Villager.die` tells its witnesses who did it.
        if (dead->type() == entity::villagers::type()) {
            mobActions_->villagerDied(
                world, *dead, murderer.has_value() ? std::optional<std::string>{murderer->toString()} : std::nullopt);
        }
        // parity: `Player.killedEntity`, the statistic of whoever landed the killing blow.
        if (murdererIsPlayer) {
            if (player::PlayerSession* killer = playerRegistry_.get(*murderer); killer != nullptr) {
                killer->stats.increment(storage::stats::killed(dead->type().toString()));
            }
        }
        // parity: `Raider.die`, a player killer becoming a hero of the village.
        if (RaidSystem::isRaider(*dead)) {
            raids_->raiderDied(world, *dead, murdererIsPlayer ? murderer : std::nullopt);
        }
        // parity: `killer.awardKillScore`, the kill credit going to the player who hit it last.
        if (dead->lastHurtByPlayer.has_value() && dead->lastHurtByPlayerTime > 0) {
            const std::lock_guard<std::mutex> guard(pendingKillsLock_);
            pendingKills_.push_back(PendingKill{.killer = *dead->lastHurtByPlayer,
                                                .victim = dead->uuid().toString(),
                                                .victimType = dead->type(),
                                                .victimFacts = world::triggerFacts::ofEntity(world, *dead),
                                                .killingBlow = killingBlowOf(world, *dead)});
        }
        const bool living = registry::entityAttributes::isLiving(dead->type());
        const bool experienceTaken = living && drinkBySculk(world, *dead);
        // parity: `Entity.die` drops the entity's own loot table where it stood.
        const bool mobDrops = world.gameRules().get(world::gameRules::kMobDrops);
        std::vector<server::world::loot::ItemStack> loot;
        std::int32_t experience = 0;
        if (!living || entity::deathDrops::dropsLoot(*dead, mobDrops)) {
            mobActions_->dropDeathInventories(world, *dead);
            loot = mobActions_->deathLootOf(world, *dead);
            if (WitherSystem::isWither(*dead)) {
                withers_->dropStar(world, *dead);
            }
        }
        if (living && !experienceTaken && entity::deathDrops::dropsExperience(*dead, mobDrops)) {
            random::RandomSource random = random::RandomSource::legacy(scratchSeed());
            experience = entity::deathDrops::experienceReward(*dead, random);
        }
        mobActions_->askDeathDrops(world, *dead, loot, experience);
        mobActions_->spawnLoot(world, *dead, loot);
        if (experience > 0) {
            const ExperienceOrbSystem::ReasonScope reason{ExperienceOrbSystem::Reason::EntityDeath, dead->uuid()};
            orbs_->award(world, dead->x, dead->y, dead->z, experience);
        }
        // parity: `AbstractMinecartContainer.remove`.
        if (const std::optional<std::int32_t> size = world::containers::sizeOf(dead->type().path());
            size.has_value() && dead->sourceNbt.has_value()) {
            random::RandomSource random = random::RandomSource::legacy(scratchSeed());
            for (const std::optional<server::world::loot::ItemStack>& slot :
                 world::blockInventory::read(*dead->sourceNbt, *size)) {
                if (slot.has_value() && slot->count > 0) {
                    world::containerDrops::dropItemStack(world.state(), dead->x, dead->y, dead->z, *slot, random,
                                                         [this] { return playerRegistry_.allocateEntityId(); });
                }
            }
        }
        // parity: `GameEvent.ENTITY_DIE`.
        world::vibrations::emit(world.state(), "entity_die", dead->x, dead->y, dead->z, world.gameTime(),
                                {.entity = dead->uuid()});
    }
}
} // namespace basalt::server::runtime
