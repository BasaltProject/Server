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
namespace {

/// parity: `TntBlock.explode`, which is where the fuse is heard being lit.
constexpr std::string_view kTntPrimedSound = "entity.tnt.primed";

[[nodiscard]] const wire::common::Identifier& playerType() {
    static const wire::common::Identifier player = wire::common::Identifier::vanilla("player");
    return player;
}

/// parity: `random.nextDouble() * (float)(Math.PI * 2)`.
constexpr double kFullTurn = 6.283185307179586;

[[nodiscard]] std::int64_t scratchSeed() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return static_cast<std::int64_t>(random());
}

[[nodiscard]] bool isTargetable(const player::PlayerSession& player) {
    // parity: `TargetingConditions.forCombat`, which skips whoever cannot be fought.
    return !player.dead && player.gamemode != wire::protocol::play::Gamemode::Creative &&
           player.gamemode != wire::protocol::play::Gamemode::Spectator;
}

[[nodiscard]] bool isSpectator(const player::PlayerSession& player) {
    return player.gamemode == wire::protocol::play::Gamemode::Spectator;
}

/// parity: `Entity.isInWater`, near enough: water where the feet are.
[[nodiscard]] bool feetInWater(world::LoadedWorld& world, const player::PlayerSession& player) {
    const std::optional<std::int32_t> state = world.state().blockState(
        wire::common::mth::floor(player.x), wire::common::mth::floor(player.y), wire::common::mth::floor(player.z));
    return state.has_value() && entity::swimPath::holdsWater(*state);
}

} // namespace

void BasaltServer::tickWorlds(const bool worldRuns) {
    server::plugins::PluginEventBus* bus = plugins_ == nullptr ? nullptr : &plugins_->events();
    static_cast<void>(
        server::plugins::emit(bus, bus == nullptr ? nullptr : &server::plugins::pluginEventTypes::serverTick(),
                              [this](server::plugins::PluginEvent& event) { event.set(0, tickLoop_->ticksRun()); }));

    // Functions first, so one that changes the world does it before this tick's own work reads it.
    // parity: `tickChildren`.
    if (worldRuns) {
        tickFunctions();
    }
    flushAdvancements();

    // Regions are worked out at the tick boundary: a chunk changing hands mid-tick is what this model cannot survive.
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        // Only when the world's chunks moved: walking every loaded chunk on a quiet tick is pure cost.
        const std::int64_t version = world->state().chunkSetVersion();
        std::map<std::string, std::int64_t>& seen = worldRegistry_->lastChunkSetVersion();
        const auto last = seen.find(world->name());
        if (last != seen.end() && last->second == version) {
            continue;
        }
        seen[world->name()] = version;
        worldRegistry_->scheduler().repartition(
            world->name(), world->state().loadedChunkKeys(), [this, world](std::int32_t merged, std::int32_t split) {
                logger_.debug([world, merged, split] {
                    return "World '" + world->name() + "': " + std::to_string(merged) + " region(s) merged, " +
                           std::to_string(split) + " split";
                });
            });
    }

    // Between turns, so nothing this thread handed out is still being walked when a departed player is released.
    playerRegistry_.collect();
    tickScoreCriteria();

    // Decided between turns: with no region running, re-homing a session cannot put two threads in one player.
    const std::map<std::string, world::LoadedWorld*> worlds = worldRegistry_->worlds();
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        const auto found = worlds.find(player->worldName);
        if (found == worlds.end()) {
            continue;
        }
        if (player->connection().handOverToRegion) {
            player->connection().handOverToRegion(*found->second);
        }
        if (player->connection().syncOpenWindow) {
            player->connection().syncOpenWindow(*found->second);
        }
    }

    worldRegistry_->scheduler().tick(
        [this](const std::exception& failure) {
            logger_.error([&failure] { return std::string{"A tick task failed: "} + failure.what(); });
        },
        [this, &worlds, worldRuns](sched::Region& region) {
            const auto found = worlds.find(region.worldName());
            if (found == worlds.end()) {
                return;
            }
            world::LoadedWorld& world = *found->second;
            const sched::FailureHandler onTaskFailure = [this, &world](const std::exception& failure) {
                logger_.error(
                    [&world, &failure] { return "A task for world '" + world.name() + "' failed: " + failure.what(); });
            };
            // This region's own queue, not the world's: another region's queue run from here put two threads in one.
            timings_.of("tick.tasks").time([&region, &onTaskFailure, worldRuns] {
                if (worldRuns) {
                    region.owner().advanceTick();
                }
                static_cast<void>(region.owner().runPending(onTaskFailure));
            });
            // `/stop` and `/restart` wind the server down before returning, so the turn ends where the server did.
            if (shuttingDown_.load()) {
                return;
            }
            const auto drainConsole = [this, &region, &onTaskFailure] {
                if (region.isPrimary() && region.worldName() == worldRegistry_->defaultWorld().name()) {
                    static_cast<void>(region.owner().runConsole(onTaskFailure));
                }
            };
            if (!worldRuns) {
                // parity: `/tick unfreeze`.
                drainConsole();
                if (shuttingDown_.load()) {
                    return;
                }
                tickDeaths(world, region);
                tickPrimedTnt(world);
                return;
            }
            tickDeaths(world, region);
            tickPrimedTnt(world);

            const auto step = [this, &world](std::string_view what, auto&& body) {
                try {
                    body();
                } catch (const std::exception& error) {
                    logger_.error([&what, &world, &error] {
                        return std::string{what} + " failed in world '" + world.name() + "': " + error.what();
                    });
                }
            };

            // Per-world state (clock, weather, players) runs once, in the world's first region.
            if (region.isPrimary()) {
                step("Time tick", [this, &world] { environment_->tickTime(world); });
                step("Border damage", [this, &world] { environment_->tickBorderDamage(world); });
                step("Border wall", [this, &world] { environment_->tickBorderWall(world); });
                step("Portal tick", [this, &world] { environment_->tickPortals(world); });
                step("Inside blocks", [this, &world] { environment_->tickInsideBlocks(world); });
                step("Location tick", [this, &world] { environment_->tickLocation(world); });
                step("Drowning tick", [this, &world] { environment_->tickSubmersion(world); });
                step("Footstep tick", [this, &world] { redstone_->tickFootsteps(world); });
                step("Warden warning tick", [this, &world] { shriekers_->tickTrackers(world); });
                step("Block test tick", [this, &world] { blockTests_->tick(world); });
            }
            step("Inhabited-time tick", [this, &world, &region] { redstone_->tickInhabitedTime(world, region); });
            // parity: `ServerLevel.tick`.
            step("Open turn", [this, &world, &region] { redstone_->openTurn(world, region); });
            step("Scheduled block tick", [this, &world, &region] { redstone_->tickScheduledBlocks(world, region); });
            step("Fluid tick", [this, &world, &region] { redstone_->tickFluids(world, region); });
            step("Random block tick", [this, &world, &region] { redstone_->tickRandomBlocks(world, region); });
            // parity: `ServerLevel.tick`.
            step("Block event tick", [this, &world, &region] { redstone_->tickBlockEvents(world, region); });
            step("Falling block tick", [this, &world, &region] { redstone_->tickFallingBlocks(world, region); });
            step("Item tick", [&world, &region] { static_cast<void>(world.state().tickItems(&region)); });
            // parity: `TntBlock.prime`.
            step("Primed tnt", [this, &world] { tickPrimedTnt(world); });
            step("Entity tick", [this, &world, &region] { tickEntities(world, region); });
            if (region.isPrimary()) {
                step("Framed map tick", [this, &world] { maps_->tickFrames(world); });
            }
            // parity: `entityInside`.
            step("Pressure plate tick", [this, &world, &region] { redstone_->tickPressurePlates(world, region); });
            step("Arrow button tick", [this, &world, &region] { redstone_->tickArrowButtons(world, region); });
            step("Tripwire tick", [this, &world, &region] { redstone_->tickTripwires(world, region); });
            // parity: `ServerLevel.tick`.
            step("Block entity tick", [this, &world, &region] { redstone_->tickBlockEntities(world, region); });
            step("Mob spawning", [this, &world, &region] { spawning_->tickSpawning(world, region); });
            if (region.isPrimary()) {
                step("Effect tick", [this, &world] { statusEffects_->tickEffects(world); });
                step("Raid tick", [this, &world] { raids_->tickRaids(world); });
                // parity: `playerList.tick`.
                drainConsole();
                if (shuttingDown_.load()) {
                    return;
                }
                step("Player tick", [this, &world] { environment_->tickPlayers(world); });
                step("Map tick", [this, &world] { maps_->tickPlayers(world); });
                step("Sleep tick", [this, &world] { environment_->tickSleep(world); });
            }
            timings_.of("tick.tasks_end").time([&region, &onTaskFailure] {
                static_cast<void>(region.owner().runEndOfTick(onTaskFailure));
            });
        });
    if (worldRuns && !shuttingDown_.load()) {
        withers_->sweepBars();
    }
    shutdownIfSignalled();
}

void BasaltServer::tickPrimedTnt(world::LoadedWorld& world) {
    std::vector<player::PlayerSession*> watchers;
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    // parity: `TntBlock.prime`.
    const registry::EntityTypeEntry* tntType = registry::entityTypes::of(entity::primedTnt::type());
    if (tntType != nullptr) {
        random::RandomSource lighting = random::RandomSource::legacy(scratchSeed());
        for (const world::PrimedTntQueue::Primed& lit : world.state().takePrimedTnt()) {
            const std::array<std::int32_t, 3> position{lit.x, lit.y, lit.z};
            static_cast<void>(entity::primedTnt::prime(world.state(), position[0], position[1], position[2],
                                                       playerRegistry_.allocateEntityId(), tntType->protocolId,
                                                       lighting.nextDouble() * kFullTurn));
            // parity: `TntBlock.prime`.
            if (world.state().hasSculkSensorNear(position[0], position[1], position[2],
                                                 world::vibrations::kCalibratedRange)) {
                world::vibrations::emit(world.state(), lit.event, position[0] + 0.5, position[1] + 0.5,
                                        position[2] + 0.5, world.gameTime());
            }
            broadcaster_->sendEffects(watchers, position[0], position[1], position[2], registry::blocks::air().stateId,
                                      {world::blockEffect::Sound{.name = std::string{kTntPrimedSound},
                                                                 .source = wire::protocol::play::SoundSource::Blocks}});
        }
    }
}

world::TriggerDamage BasaltServer::killingBlowOf(const world::LoadedWorld& world, const entity::ServerEntity& dead) {
    world::TriggerDamage blow{.type = world::triggerFacts::damageTypeOf(dead.lastDamageCause)};
    const auto factsOf = [this, &world](std::int32_t id) -> std::optional<server::world::loot::EntityFacts> {
        for (const player::PlayerSession* player : playerRegistry_.snapshot()) {
            if (player->entityId() == id) {
                return world::triggerFacts::ofPlayer(*player);
            }
        }
        const entity::ServerEntity* mob = world.state().entity(id);
        return mob == nullptr ? std::nullopt : std::optional{world::triggerFacts::ofEntity(world, *mob)};
    };
    const entity::ServerEntity::Blame& blame = dead.lastBlame;
    if (blame.causingId.has_value()) {
        blow.source = factsOf(*blame.causingId);
    }
    if (blame.directId.has_value()) {
        blow.direct = factsOf(*blame.directId);
    }
    if (!blow.direct.has_value() && blame.directType.has_value()) {
        blow.direct = server::world::loot::EntityFacts{.type = *blame.directType};
    }
    blow.directHit = blame.causingId.has_value() && blame.causingId == blame.directId;
    return blow;
}

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

void BasaltServer::tickEntities(world::LoadedWorld& world, const sched::Region& region) {
    std::vector<player::PlayerSession*> watchers;
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    // parity: `LivingEntity.pushEntities` on each player's own turn.
    for (player::PlayerSession* player : watchers) {
        if (region.ownsPosition(player->x, player->z)) {
            // parity: `baseTick`'s currents come before `aiStep`'s pushes.
            pushes_->pushByFluid(world, *player);
            pushes_->pushByPlayer(world, *player);
        }
    }

    // parity: `BlockEntity.preRemoveSideEffects`.
    // parity: `StructureBlock.trigger`.
    for (const std::array<std::int32_t, 3>& at : world.state().takeStructureTriggers()) {
        if (structureBlocks_ != nullptr) {
            structureBlocks_->trigger(world, at);
        }
    }
    for (world::WorldState::Spilled& spilled : world.state().takeSpilledContents()) {
        if (spilled.lootTable.has_value()) {
            world::containerLoot::fill(world, vanillaData_.lootTables, *spilled.lootTable, spilled.lootSeed,
                                       spilled.lootSlots, false, {spilled.x, spilled.y, spilled.z});
            for (std::optional<server::world::loot::ItemStack>& slot : spilled.lootSlots) {
                if (slot.has_value() && slot->count > 0) {
                    spilled.stacks.push_back(std::move(*slot));
                }
            }
        }
        random::RandomSource random = random::RandomSource::legacy(scratchSeed());
        for (const server::world::loot::ItemStack& stack : spilled.stacks) {
            // parity: `dropItemStack` from the square, not its centre, in handfuls.
            world::containerDrops::dropItemStack(world.state(), spilled.x, spilled.y, spilled.z, stack, random,
                                                 [this] { return playerRegistry_.allocateEntityId(); });
        }
        // parity: `AbstractFurnaceBlockEntity.preRemoveSideEffects` pays the cooking out at the centre.
        for (const auto& [name, count] : spilled.recipesUsed) {
            const std::expected<wire::common::Identifier, wire::common::IdentifierError> id =
                wire::common::Identifier::parse(name);
            const server::world::crafting::Recipe* recipe =
                id.has_value() ? vanillaData_.recipes.recipeFor(*id) : nullptr;
            if (recipe != nullptr) {
                orbs_->award(world, spilled.x + 0.5, spilled.y + 0.5, spilled.z + 0.5,
                             world::furnaces::experienceFor(count, recipe->experience, random));
            }
        }
    }

    const entity::entityNbt::AllocateEntityId allocateEntityId = [this] { return playerRegistry_.allocateEntityId(); };
    const entity::entityNbt::AllocateEntityIdFor allocateEntityIdFor = [this](const wire::common::Identifier& type) {
        return playerRegistry_.allocateEntityIds(entity::enderDragon::idSpan(type));
    };
    const entity::entityNbt::ProtocolTypeOf protocolTypeOf =
        [](const wire::common::Identifier& type) -> std::optional<std::int32_t> {
        const registry::EntityTypeEntry* entry = registry::entityTypes::of(type);
        return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
    };
    // Entity ids are handed out by the tick loop, while chunks load on a generation thread.
    for (const wire::nbt::NbtCompound& tag : world.state().takePendingEntities()) {
        // An item on the floor is its own thing here, not a `ServerEntity`, so it comes back its own way.
        if (const std::optional<world::DroppedItem> item = world::droppedItemNbt::decode(tag, allocateEntityId);
            item.has_value()) {
            static_cast<void>(world.state().spawnItem(*item));
            continue;
        }
        for (std::unique_ptr<entity::ServerEntity>& restored :
             entity::entityNbt::decodeTree(tag, allocateEntityIdFor, protocolTypeOf)) {
            static_cast<void>(world.state().spawnEntity(std::move(restored)));
        }
    }
    // Structure mobs are generated off-thread and cannot allocate runtime ids, so they become live entities here.
    for (const server::world::ChunkGeneratedEntity& descriptor : world.state().takePendingGeneratedEntities()) {
        for (std::unique_ptr<entity::ServerEntity>& generated :
             entity::entityNbt::decodeGeneratedTree(descriptor, allocateEntityIdFor, protocolTypeOf)) {
            static_cast<void>(world.state().spawnEntity(std::move(generated)));
        }
    }

    // parity: `LivingEntity.tickDeath`.
    for (entity::ServerEntity* dying : world.state().entities()) {
        // parity: `EnderDragon.tickDeath` is its own two hundred ticks, run from its own turn.
        if (dying->removed || !dying->dying || dying->dragon != nullptr) {
            continue;
        }
        ++dying->deathTime;
        // parity: `Creaking.tickDeath`.
        if (dying->creaking != nullptr && dying->creaking->tearingDown && dying->creaking->home.has_value()) {
            if (dying->deathTime > entity::creakings::kTearDownTicks && mobActions_ != nullptr) {
                mobActions_->tearDownCreaking(world, *dying);
            }
            continue;
        }
        if (dying->deathTime < entity::kDeathTicks) {
            continue;
        }
        // parity: `AbstractCubeMob.remove`.
        random::RandomSource splitRandom = random::RandomSource::legacy(scratchSeed());
        for (const entity::cubeMobs::Split& split : entity::cubeMobs::splitsOf(*dying, splitRandom)) {
            if (spawning_ == nullptr) {
                break;
            }
            static_cast<void>(spawning_->spawnMob(
                world, dying->type(), split.x, split.y, split.z, split.yaw, "triggered",
                [&split](entity::ServerEntity& half) { entity::cubeMobs::applySize(half, split.size); }));
        }
        dying->removed = true;
    }

    // One snapshot per tick: `entities()` copies under the world lock and per-entity calls made a lock convoy.
    std::vector<entity::ServerEntity*> living;
    for (entity::ServerEntity* candidate : world.state().entities()) {
        if (!candidate->removed) {
            living.push_back(candidate);
        }
    }

    // parity: `TargetingConditions.forCombat`.
    const double playerWidth = registry::entityTypeShapes::widthOf(playerType());
    const double playerHeight = registry::entityTypeShapes::heightOf(playerType());
    std::vector<entity::ai::PlayerView> views;
    for (player::PlayerSession* player : watchers) {
        if (region.ownsPosition(player->x, player->z)) {
            player->movedHorizontally =
                std::hypot(player->x - player->seenAt[0], player->z - player->seenAt[1]) > static_cast<double>(1.0E-5F);
            player->seenAt = {player->x, player->z};
            std::optional<wire::common::Identifier> heldItem;
            const std::optional<storage::StoredItemStack>& held = player->inventory.heldItem();
            if (held.has_value()) {
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                    wire::common::Identifier::parse(held->id);
                if (parsed.has_value()) {
                    heldItem = *parsed;
                }
            }
            const std::optional<storage::StoredItemStack>& head =
                player->inventory.at(player::PlayerInventory::kArmourStart);
            const auto headItem = wire::common::Identifier::parse(head.has_value() ? head->id : std::string{});
            const bool disguised =
                headItem.has_value() &&
                world::realItemTags::contains(vanillaData_.itemTags, "minecraft:gaze_disguise_equipment", *headItem);
            views.push_back(entity::ai::PlayerView{
                .entityId = player->entityId(),
                .uuid = player->uuid().toString(),
                .x = player->x,
                .y = player->y,
                .z = player->z,
                .width = playerWidth,
                .height = playerHeight,
                .targetable = isTargetable(*player),
                .spectator = isSpectator(*player),
                .heldItem = std::move(heldItem),
                .sneaking = player->sneaking,
                .sprinting = player->sprinting,
                .swimming = player->swimming,
                .yaw = player->yaw,
                .pitch = player->pitch,
                .vehicleId = player->vehicleId,
                .movedHorizontally = player->movedHorizontally,
                .facing = wire::common::mth::floor(static_cast<double>(player->yaw / 90.0F) + 0.5) & 3,
                .sleeping = player->sleepingBed.has_value(),
                .sleepTicks = player->sleepTicks,
                .gazeDisguised = disguised,
                .flying = player->flying,
                .inWater = feetInWater(world, *player)});
        }
    }

    if (region.isPrimary()) {
        orbs_->tickPlayers(watchers);
        if (mobActions_ != nullptr) {
            mobActions_->forgiveDeadPlayers(world, watchers);
            mobActions_->tickShoulders(world, watchers);
            mobActions_->emitPlayerMovement(world, watchers);
        }
    }
    // parity: `ServerLevel.tick` runs the dragon fight before any entity takes its turn.
    if (world.dragonFight() != nullptr && region.ownsPosition(0.0, 0.0)) {
        endFight_->tickFight(world, watchers, living);
    }

    random::RandomSource random = random::RandomSource::legacy(scratchSeed());
    const world::cushions::Tagged attachedTags = [this](std::string_view tag, const wire::common::Identifier& block) {
        return world::realBlockTags::contains(vanillaData_.blockTags, tag, block);
    };
    // Built lazily; entities stop one chunk sooner than blocks (`ChunkLevel.byStatus`).
    std::optional<std::unordered_set<std::int64_t>> entityTicking;
    const auto ticking = [this, &entityTicking, &world, &region](double atX, double atZ) {
        if (!entityTicking.has_value()) {
            std::unordered_set<std::int64_t> built;
            for (const std::array<std::int32_t, 2>& at : chunkSimulation_.simulatedChunks(world, &region)) {
                built.insert((static_cast<std::int64_t>(at[0]) << 32) | static_cast<std::uint32_t>(at[1]));
            }
            entityTicking = std::move(built);
        }
        const auto chunkX = static_cast<std::int32_t>(std::floor(atX)) >> 4;
        const auto chunkZ = static_cast<std::int32_t>(std::floor(atZ)) >> 4;
        return entityTicking->contains((static_cast<std::int64_t>(chunkX) << 32) | static_cast<std::uint32_t>(chunkZ));
    };
    for (entity::ServerEntity* entity : living) {
        if (entity->removed || MobTickSystem::hasActiveVehicle(world, *entity)) {
            continue;
        }
        // parity: `Mob.aiStep`.
        if (entity->dying && !entity::enderDragon::isDragon(entity->type())) {
            // parity: a dying wither is still there, and so is its bar, until the body goes.
            if (WitherSystem::isWither(*entity)) {
                withers_->keepBar(world, *entity);
            }
            continue;
        }
        // An entity may walk out of this region mid-turn; repartitioning only happens between ticks, so that is fine.
        if (!region.ownsPosition(entity->x, entity->z)) {
            continue;
        }
        // parity: `ServerLevel`.
        if (!ticking(entity->x, entity->z)) {
            continue;
        }
        if (entity->type() == world::lightning::bolt()) {
            lightning_->tick(world, watchers, living, *entity, random);
            continue;
        }
        if (entity->type() == entity::primedTnt::type()) {
            if (!entity::primedTnt::tick(world.state(), *entity)) {
                continue;
            }
            entity->removed = true;
            explosions_->blowUp(world, watchers, entity->x, entity->y + entity::primedTnt::kExplosionOffset, entity->z,
                                entity::primedTnt::kExplosionPower, "tnt", entity->entityId());
            // parity: `GameEvent.EXPLODE`, which is fifteen: the top of the scale.
            world::vibrations::emit(world.state(), "explode", entity->x, entity->y, entity->z, world.gameTime(),
                                    {.entity = entity->uuid()});
            continue;
        }
        if (entity::enderDragon::isDragon(entity->type())) {
            endFight_->tickDragon(world, watchers, living, *entity);
            continue;
        }
        if (ExperienceOrbSystem::isOrb(*entity)) {
            orbs_->tick(world, watchers, living, *entity);
            continue;
        }
        if (entity::areaEffectClouds::isCloud(entity->type())) {
            clouds_->tick(world, watchers, living, *entity);
            continue;
        }
        if (WitherSystem::isWither(*entity) && !entity->dying) {
            withers_->beforeTurn(world, living, *entity);
            mobTicks_->tickTree(world, *entity, watchers, views, living, random);
            withers_->afterTurn(world, watchers, living, *entity);
            continue;
        }
        if (FishingSystem::isHook(*entity)) {
            fishing_->tick(world, watchers, living, *entity);
            continue;
        }
        // parity: `EndCrystal.tick`.
        if (world::endCrystals::isCrystal(entity->type())) {
            if (world.dragonFight() != nullptr) {
                const std::int32_t delay = world::fires::kMinDelay + random.nextInt(world::fires::kDelaySpread);
                if (const std::optional<std::int32_t> lit =
                        world::endCrystals::keepFireLit(world.state(), *entity, delay);
                    lit.has_value()) {
                    broadcaster_->announceRedstone(world, watchers,
                                                   world::BlockChange{static_cast<std::int32_t>(std::floor(entity->x)),
                                                                      static_cast<std::int32_t>(std::floor(entity->y)),
                                                                      static_cast<std::int32_t>(std::floor(entity->z)),
                                                                      *lit,
                                                                      {}});
                }
            }
            continue;
        }
        if (world::enderEyes::isEye(*entity)) {
            world::enderEyes::tick(world, watchers, playerRegistry_, *entity, scratchSeed());
            continue;
        }
        if (entity::fireworkRockets::isRocket(*entity)) {
            fireworks_->tick(world, watchers, living, *entity);
            continue;
        }
        if (entity::shulkers::isBullet(entity->type())) {
            mobActions_->tickShulkerBullet(world, watchers, living, *entity, random);
            continue;
        }
        const entity::projectiles::Kind* projectile = entity::projectiles::kindOf(entity->type());
        if (projectile != nullptr) {
            combat_->tickProjectile(world, watchers, living, *entity, *projectile);
            // parity: a pearl thrown through a gateway comes out at its exit and stops dead.
            if (!entity->removed && entity->type().path() == "ender_pearl") {
                const double half = entity->width() / 2.0;
                if (const std::optional<std::array<double, 3>> exit = environment_->enterGateway(
                        world, {entity->x - half, entity->y, entity->z - half, entity->x + half,
                                entity->y + entity->height(), entity->z + half});
                    exit.has_value()) {
                    entity->x = (*exit)[0];
                    entity->y = (*exit)[1];
                    entity->z = (*exit)[2];
                    entity->velocityX = 0.0;
                    entity->velocityY = 0.0;
                    entity->velocityZ = 0.0;
                }
            }
            continue;
        }
        // parity: `AbstractMinecart.tick`: on a rail it follows the track, and off one it falls.
        if (world::detectorRails::isMinecart(entity->type())) {
            vehicles_->tickMinecart(world, *entity);
            if (entity->removed) {
                continue;
            }
            player::PlayerSession* rider = nullptr;
            for (player::PlayerSession* player : watchers) {
                if (!entity->passengerIds.empty() && player->entityId() == entity->passengerIds.front()) {
                    rider = player;
                    break;
                }
            }
            // The rider's position comes from the cart, so chunks load around the same answer the client draws.
            if (rider != nullptr) {
                rider->x = entity->x;
                rider->y = entity->y;
                rider->z = entity->z;
            }
            // parity: `MinecartHopper.tick`.
            static_cast<void>(world::hoppers::tickCart(world.state(), *entity, world.gameTime()));
            const std::int32_t cartY = wire::common::mth::floor(entity->y);
            for (std::int32_t railX = wire::common::mth::floor(entity->x - world::detectorRails::kCartHalfWidth);
                 railX <= wire::common::mth::floor(entity->x + world::detectorRails::kCartHalfWidth); ++railX) {
                for (std::int32_t railZ = wire::common::mth::floor(entity->z - world::detectorRails::kCartHalfWidth);
                     railZ <= wire::common::mth::floor(entity->z + world::detectorRails::kCartHalfWidth); ++railZ) {
                    if (const world::detectorRails::Checked pressed = world::detectorRails::check(
                            world.state(), railX, cartY, railZ, living, world::detectorRails::Caller::Entity);
                        pressed.ran) {
                        if (pressed.change.has_value()) {
                            broadcaster_->announceRedstone(world, watchers, *pressed.change);
                        }
                        // parity: `checkPressed` tells the comparators whatever it decided.
                        world::redstoneComponents::scheduleComparatorsAround(
                            world.state(), world.name(), railX, cartY, railZ,
                            world::comparatorReading::of(world.state()));
                    }
                }
            }
            std::set<std::int32_t> visited;
            mobTicks_->tickPassengers(world, *entity, watchers, views, living, random, visited);
            continue;
        }
        // parity: `BlockAttachedEntity.tick`.
        if (world::attachedEntities::isAttached(entity->type()) && world::attachedEntities::dueForCheck(*entity) &&
            !world::attachedEntities::survives(world.state(), *entity, attachedTags)) {
            // parity: `ItemFrame.dropItem(level, null, true)` drops the framed map's marker either way.
            if (maps_ != nullptr && world::itemFrames::isFrame(entity->type())) {
                if (const std::optional<server::world::loot::ItemStack> held = world::itemFrames::shownIn(*entity);
                    held.has_value()) {
                    maps_->frameLostMap(held->components, *entity);
                }
            }
            world::attachedEntities::comeOff(world, *entity, watchers, random,
                                             [this] { return playerRegistry_.allocateEntityId(); });
            continue;
        }
        // parity: `AbstractBoat.tick`.
        if (entity::boats::isBoat(entity->type())) {
            tickBoat(world, watchers, living, *entity);
            if (mobActions_ != nullptr) {
                mobActions_->tickLeash(world, *entity);
            }
        }
        // parity: `Mob.aiStep`, which is what makes an animal wander rather than stand there.
        if (entity::mobs::isMob(entity->type())) {
            mobTicks_->tickTree(world, *entity, watchers, views, living, random);
        } else {
            std::set<std::int32_t> visited;
            mobTicks_->tickPassengers(world, *entity, watchers, views, living, random, visited);
        }
    }
    // Deaths land this turn, not the next: an arrow killing a pig is heard by vanilla's sensor on the same tick.
    tickDeaths(world, region);
}

} // namespace basalt::server::runtime
