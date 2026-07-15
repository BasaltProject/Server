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

} // namespace basalt::server::runtime
