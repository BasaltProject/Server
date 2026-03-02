#include <basalt/server/plugins/PluginEventTypes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/registry/Entities.hpp>
#include <basalt/server/registry/Registries.hpp>
#include <basalt/server/runtime/BasaltServer.hpp>
#include <basalt/server/runtime/entity/EntityNbt.hpp>
#include <basalt/server/runtime/entity/Minecarts.hpp>
#include <basalt/server/runtime/entity/MobCombat.hpp>
#include <basalt/server/runtime/entity/MobRules.hpp>
#include <basalt/server/runtime/entity/PrimedTnt.hpp>
#include <basalt/server/runtime/entity/Projectiles.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Containers.hpp>
#include <basalt/server/runtime/world/DetectorRails.hpp>
#include <basalt/server/runtime/world/DroppedItemNbt.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>
#include <basalt/server/runtime/world/Lightning.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
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

/// The player type, whose box every mob measures a player by.
[[nodiscard]] const wire::common::Identifier& playerType() {
    static const wire::common::Identifier player = wire::common::Identifier::vanilla("player");
    return player;
}

/// parity: `random.nextDouble() * (float)(Math.PI * 2)`, the whole turn a lit block of tnt picks its
/// little sideways push from.
constexpr double kFullTurn = 6.283185307179586;

/// The seed of the tick's own random. Nothing here is checked bit for bit against vanilla: what a
/// mob decides this turn is not part of a seed's world.
[[nodiscard]] std::int64_t scratchSeed() {
    static thread_local std::mt19937_64 random{std::random_device{}()};
    return static_cast<std::int64_t>(random());
}

[[nodiscard]] bool isTargetable(const player::PlayerSession& player) {
    // parity: `TargetingConditions.forCombat`, which skips whoever cannot be fought.
    return !player.dead && player.gamemode != wire::protocol::play::Gamemode::Creative &&
           player.gamemode != wire::protocol::play::Gamemode::Spectator;
}

} // namespace

void BasaltServer::tickWorlds(const bool worldRuns) {
    // Told once a tick, and only if somebody asked: with no listener this is a boolean.
    server::plugins::PluginEventBus* bus = plugins_ == nullptr ? nullptr : &plugins_->events();
    static_cast<void>(
        server::plugins::emit(bus, bus == nullptr ? nullptr : &server::plugins::pluginEventTypes::serverTick(),
                              [this](server::plugins::PluginEvent& event) { event.set(0, tickLoop_->ticksRun()); }));

    // Scheduled functions first, so one that changes the world does it before this tick's own work
    // reads it. parity: vanilla ticks its function timer before the levels.
    for (const wire::common::Identifier& id : functionSchedule_.tick()) {
        try {
            static_cast<void>(functionRunner_->run(*wired_.dispatcher, *consoleSource_, id));
        } catch (const std::exception& error) {
            logger_.error(
                [&id, &error] { return "Scheduled function '" + id.toString() + "' failed: " + error.what(); });
        }
    }

    // What the world has loaded decides what its regions are, and it is worked out here, at the tick
    // boundary, before any of them starts: a chunk changing hands mid-tick is the one thing this
    // model cannot survive. With regions off it does nothing.
    for (const auto& [name, world] : worldRegistry_->worlds()) {
        // Only when the world's chunks actually moved: the shape of a world is decided by them, and
        // walking every loaded chunk on a tick where none arrived or left is pure cost.
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

    // Between turns, where nothing this thread handed out is still being walked: what a player who
    // left leaves behind is let go here and nowhere else.
    playerRegistry_.collect();

    // Where each player's packets go is decided here, between turns: no region is running, so
    // handing a session that walked across a border to its new owner cannot land two threads inside
    // the same player.
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

    // Every turn goes through the scheduler, in both modes: the global owner first and then each
    // region. With the world in one piece that is one region per world, ticked in the order the
    // worlds were loaded, which is the order this loop walked them in before.
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
            // A turn is: what came due, then what was waiting, then the world's own work, and last
            // what asked to run at the end of it. Everything below this line is that third part.
            // This region's own queue, not the world's: once a world is split there are several, and
            // running the first region's queue from every region's thread was two threads in the
            // same queue at once.
            timings_.of("tick.tasks").time([&region, &onTaskFailure, worldRuns] {
                // parity: a frozen turn does not move the clock, so nothing waiting on a delay comes
                // due, but what is already queued still runs: that is how a console command lands
                // while `/tick freeze` holds.
                if (worldRuns) {
                    region.owner().advanceTick();
                }
                static_cast<void>(region.owner().runPending(onTaskFailure));
            });
            // `/stop` and `/restart` run on this queue and wind the whole server down before they
            // return: players gone, worlds written, scheduler closed. What follows would give a turn
            // to a world that is already saved and shut, so the turn ends where the server did.
            if (shuttingDown_.load()) {
                return;
            }
            // Whatever those commands killed or lit goes off now, frozen or not.
            //
            // KNOWN GAP, not parity: vanilla runs queued tasks after `processPacketsAndTick`, so a
            // console command lands after the world's turn, and `tickDeath` and a primed tnt are
            // ordinary entity ticks there. Moving this needs its own measurement first.
            tickDeaths(world, region);
            tickPrimedTnt(world);
            if (!worldRuns) {
                return;
            }

            const auto step = [this, &world](std::string_view what, auto&& body) {
                try {
                    body();
                } catch (const std::exception& error) {
                    logger_.error([&what, &world, &error] {
                        return std::string{what} + " failed in world '" + world.name() + "': " + error.what();
                    });
                }
            };

            // What belongs to a piece of the map is done by whoever owns that piece; what belongs to
            // the world as a whole (its clock, its weather, its players) runs once, in the world's
            // first region. With one region the two are the same thing.
            if (region.isPrimary()) {
                step("Time tick", [this, &world] { environment_->tickTime(world); });
                step("Border damage", [this, &world] { environment_->tickBorderDamage(world); });
                step("Portal tick", [this, &world] { environment_->tickPortals(world); });
                step("Drowning tick", [this, &world] { environment_->tickSubmersion(world); });
                step("Footstep tick", [this, &world] { redstone_->tickFootsteps(world); });
            }
            step("Inhabited-time tick", [this, &world, &region] { redstone_->tickInhabitedTime(world, region); });
            // parity: `ServerLevel.tick` runs the booked block turns first, the fluids after them and
            // the random ticks last, inside the chunk source. Doing the water first let it break a
            // repeater whose own turn was due that very tick, and the pulse the jar gives was lost.
            // parity: the jar books an absolute `triggerTick`, so a turn cannot shorten what it
            // books itself. This is that, said with a countdown: everything counts down once, here,
            // before a single turn runs. Inside a pass instead, the fluids lost a tick of whatever
            // the block turns had just booked, measured in `vibranota`.
            step("Open turn", [this, &world, &region] { redstone_->openTurn(world, region); });
            step("Scheduled block tick", [this, &world, &region] { redstone_->tickScheduledBlocks(world, region); });
            step("Fluid tick", [this, &world, &region] { redstone_->tickFluids(world, region); });
            step("Random block tick", [this, &world, &region] { redstone_->tickRandomBlocks(world, region); });
            // parity: `ServerLevel.tick` runs its block events after the block ticks and before the
            // entities, and skips them altogether while the world is frozen.
            step("Block event tick", [this, &world, &region] { redstone_->tickBlockEvents(world, region); });
            step("Falling block tick", [this, &world, &region] { redstone_->tickFallingBlocks(world, region); });
            // parity: a dropped item is an entity like any other and moves in the entity pass, not
            // before the world's own turn: what a block event pushes is where the item was, not
            // where it is about to be.
            step("Item tick", [&world, &region] { static_cast<void>(world.state().tickItems(&region)); });
            // parity: `TntBlock.prime` builds the entity where the signal reached it, inside the
            // block's own turn, so it is already in the world for **this** tick's entity pass and
            // moves with the hop its constructor gave it. Draining the queue only at the top of the
            // turn left every lit block of tnt a tick behind the jar's. Measured in `hojagrande`.
            step("Primed tnt", [this, &world] { tickPrimedTnt(world); });
            step("Entity tick", [this, &world, &region] { tickEntities(world, region); });
            // parity: `entityInside` is part of the entity's own turn, so it reads where the entity
            // has just moved to. Asking before that pass read the square of the turn before, which a
            // dropped item hides (they move first of all) and an arrow does not: a tick late.
            step("Pressure plate tick", [this, &world, &region] { redstone_->tickPressurePlates(world, region); });
            step("Arrow button tick", [this, &world, &region] { redstone_->tickArrowButtons(world, region); });
            step("Tripwire tick", [this, &world, &region] { redstone_->tickTripwires(world, region); });
            // parity: `ServerLevel.tick` leaves `tickBlockEntities` for the **end**, after the
            // entities. A hopper minecart parked over a hopper is the visible half: the cart takes
            // the item on its own turn and the hopper below it can already take it that same tick.
            step("Block entity tick", [this, &world, &region] { redstone_->tickBlockEntities(world, region); });
            step("Mob spawning", [this, &world, &region] { spawning_->tickSpawning(world, region); });
            if (region.isPrimary()) {
                step("Effect tick", [this, &world] { statusEffects_->tickEffects(world); });
                step("Raid tick", [this, &world] { raids_->tickRaids(world); });
                step("Player tick", [this, &world] { environment_->tickPlayers(world); });
                step("Sleep tick", [this, &world] { environment_->tickSleep(world); });
            }
            // The end of the turn: what has to see everything it did, rather than a tick late.
            timings_.of("tick.tasks_end").time([&region, &onTaskFailure] {
                static_cast<void>(region.owner().runEndOfTick(onTaskFailure));
            });
        });
    shutdownIfSignalled();
}

void BasaltServer::tickPrimedTnt(world::LoadedWorld& world) {
    std::vector<player::PlayerSession*> watchers;
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }
    // parity: `TntBlock.prime` builds the entity **inside** the neighbour update that lit the block,
    // so the fuse is heard on that very turn. The block half of it can run on a client's thread, so
    // what it does is queue the square; this is where the queue becomes an entity, and it runs before
    // the world's own turn so the two land together.
    const registry::EntityTypeEntry* tntType = registry::entityTypes::of(entity::primedTnt::type());
    if (tntType != nullptr) {
        random::RandomSource lighting = random::RandomSource::legacy(scratchSeed());
        for (const world::PrimedTntQueue::Primed& lit : world.state().takePrimedTnt()) {
            const std::array<std::int32_t, 3> position{lit.x, lit.y, lit.z};
            // parity: the turn of that push comes off the level's random, which this build does not
            // share with the jar: the hop is the same and the drift is noise.
            static_cast<void>(entity::primedTnt::prime(world.state(), position[0], position[1], position[2],
                                                       playerRegistry_.allocateEntityId(), tntType->protocolId,
                                                       lighting.nextDouble() * kFullTurn));
            // parity: `TntBlock.prime` raises `PRIME_FUSE` from the block's own square, so a sculk
            // sensor hears the tnt being lit at ten and not only the bang at fifteen.
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

void BasaltServer::tickDeaths(world::LoadedWorld& world, const sched::Region& region) {
    for (entity::ServerEntity* dead : world.state().entities()) {
        if (!dead->killed || !region.ownsPosition(dead->x, dead->z)) {
            continue;
        }
        dead->killed = false;
        // parity: `Entity.die` drops the entity's own loot table where it stood.
        mobActions_->dropDeathLoot(world, *dead);
        // parity: `AbstractMinecartContainer.remove` calls `Containers.dropContents`, so what the
        // cart was carrying falls where it was killed. Measured in `vagonetacomparador`: the jar
        // leaves 192 stone and we left nothing. The drop is `dropItemStack`'s, which starts from the
        // floor of the square (`Math.floor`) and not from the entity's exact point.
        if (const std::optional<std::int32_t> size = world::containers::sizeOf(dead->type().path());
            size.has_value() && dead->sourceNbt.has_value()) {
            random::RandomSource random = random::RandomSource::legacy(scratchSeed());
            constexpr double kItemWidth = 0.25;
            constexpr double kRange = 1.0 - kItemWidth;
            constexpr double kSpread = 0.11485000171139836;
            for (const std::optional<server::world::loot::ItemStack>& slot :
                 world::blockInventory::read(*dead->sourceNbt, *size)) {
                if (!slot.has_value() || slot->count <= 0) {
                    continue;
                }
                static_cast<void>(world.state().spawnItem(
                    world::DroppedItem{.entityId = playerRegistry_.allocateEntityId(),
                                       .uuid = wire::common::Uuid::random(),
                                       .item = slot->item,
                                       .count = slot->count,
                                       .x = std::floor(dead->x) + (random.nextDouble() * kRange) + (kItemWidth / 2.0),
                                       .y = std::floor(dead->y) + (random.nextDouble() * kRange),
                                       .z = std::floor(dead->z) + (random.nextDouble() * kRange) + (kItemWidth / 2.0),
                                       .velocityX = random.triangle(0.0, kSpread),
                                       .velocityY = random.triangle(0.2, kSpread),
                                       .velocityZ = random.triangle(0.0, kSpread)}));
            }
        }
        // parity: `GameEvent.ENTITY_DIE`, the loudest thing a sculk sensor hears. Measured in
        // `sensormuerte`: waiting for the next turn to raise it made the sensor wake a tick late,
        // because a `/kill` runs while the world is frozen and the jar stamps it with that turn.
        world::vibrations::emit(world.state(), "entity_die", dead->x, dead->y, dead->z, world.gameTime());
    }
}

void BasaltServer::tickEntities(world::LoadedWorld& world, const sched::Region& region) {
    std::vector<player::PlayerSession*> watchers;
    for (player::PlayerSession* player : playerRegistry_.snapshot()) {
        if (player->worldName == world.name()) {
            watchers.push_back(player);
        }
    }

    // parity: `BlockEntity.preRemoveSideEffects` over `Containers.dropItemStack`: what a container
    // held falls out where its block stood. The block write queues it because it holds the world's
    // lock; this is where it becomes items, on the same thread that owns them.
    for (const world::WorldState::Spilled& spilled : world.state().takeSpilledContents()) {
        random::RandomSource random = random::RandomSource::legacy(scratchSeed());
        for (const server::world::loot::ItemStack& stack : spilled.stacks) {
            // parity: the scatter is the item's own width taken off each side, and the drop is
            // **not** centred on the square: `dropItemStack` adds half a width on x and z and none
            // on y.
            constexpr double kItemWidth = 0.25;
            constexpr double kRange = 1.0 - kItemWidth;
            const double x = spilled.x + (random.nextDouble() * kRange) + (kItemWidth / 2.0);
            const double y = spilled.y + (random.nextDouble() * kRange);
            const double z = spilled.z + (random.nextDouble() * kRange) + (kItemWidth / 2.0);
            constexpr double kSpread = 0.11485000171139836;
            static_cast<void>(world.state().spawnItem(world::DroppedItem{.entityId = playerRegistry_.allocateEntityId(),
                                                                         .uuid = wire::common::Uuid::random(),
                                                                         .item = stack.item,
                                                                         .count = stack.count,
                                                                         .x = x,
                                                                         .y = y,
                                                                         .z = z,
                                                                         .velocityX = random.triangle(0.0, kSpread),
                                                                         .velocityY = random.triangle(0.2, kSpread),
                                                                         .velocityZ = random.triangle(0.0, kSpread)}));
        }
    }

    const entity::entityNbt::AllocateEntityId allocateEntityId = [this] { return playerRegistry_.allocateEntityId(); };
    const entity::entityNbt::ProtocolTypeOf protocolTypeOf =
        [](const wire::common::Identifier& type) -> std::optional<std::int32_t> {
        const registry::EntityTypeEntry* entry = registry::entityTypes::of(type);
        return entry == nullptr ? std::nullopt : std::optional{entry->protocolId};
    };
    // The mobs a chunk brought back from disk: they wait for the tick loop because an entity id is
    // the server's to hand out, and a chunk is loaded on a generation thread.
    for (const wire::nbt::NbtCompound& tag : world.state().takePendingEntities()) {
        // An item on the floor is its own thing here, not a `ServerEntity`, so it comes back its
        // own way. parity: to vanilla they are both just entities saved with the chunk.
        if (const std::optional<world::DroppedItem> item = world::droppedItemNbt::decode(tag, allocateEntityId);
            item.has_value()) {
            static_cast<void>(world.state().spawnItem(*item));
            continue;
        }
        for (std::unique_ptr<entity::ServerEntity>& restored :
             entity::entityNbt::decodeTree(tag, allocateEntityId, protocolTypeOf)) {
            static_cast<void>(world.state().spawnEntity(std::move(restored)));
        }
    }
    // Structure templates are generated off-thread and cannot allocate runtime ids there. A mob
    // descriptor becomes the same live entity as a spawn egg here, so it receives AI, movement,
    // persistence and ordinary client synchronization instead of remaining scenery.
    for (const server::world::ChunkGeneratedEntity& descriptor : world.state().takePendingGeneratedEntities()) {
        for (std::unique_ptr<entity::ServerEntity>& generated :
             entity::entityNbt::decodeGeneratedTree(descriptor, allocateEntityId, protocolTypeOf)) {
            static_cast<void>(world.state().spawnEntity(std::move(generated)));
        }
    }

    // parity: `LivingEntity.tickDeath`, which is the whole of what a dead mob does: count to twenty
    // and then go. Until it goes it is still an entity, so a sculk sensor, a pressure plate and a
    // comparator all keep feeling it, and `/kill` is no exception.
    for (entity::ServerEntity* dying : world.state().entities()) {
        if (dying->removed || !dying->dying) {
            continue;
        }
        ++dying->deathTime;
        if (dying->deathTime >= entity::kDeathTicks) {
            dying->removed = true;
        }
    }

    // One snapshot for the whole tick: `entities()` takes the world's lock and copies the list, and
    // asking for it per entity turned a pen of cows into a lock convoy.
    //
    // A dying mob stays on the list: it is still an entity with a box, so a detector rail under it
    // still reads it. What it stops doing is its own turn, which is skipped below.
    std::vector<entity::ServerEntity*> living;
    for (entity::ServerEntity* candidate : world.state().entities()) {
        if (!candidate->removed) {
            living.push_back(candidate);
        }
    }

    // parity: `TargetingConditions.forCombat`, which skips anybody a mob cannot fight. Only the
    // players in this region: anybody else is at least a section away, which is further than any
    // mob's follow range, and building the list for every region was a hundred copies of it.
    const double playerWidth = registry::entityTypeShapes::widthOf(playerType());
    const double playerHeight = registry::entityTypeShapes::heightOf(playerType());
    std::vector<entity::ai::PlayerView> views;
    for (player::PlayerSession* player : watchers) {
        if (region.ownsPosition(player->x, player->z)) {
            std::optional<wire::common::Identifier> heldItem;
            const std::optional<storage::StoredItemStack>& held = player->inventory.heldItem();
            if (held.has_value()) {
                const std::expected<wire::common::Identifier, wire::common::IdentifierError> parsed =
                    wire::common::Identifier::parse(held->id);
                if (parsed.has_value()) {
                    heldItem = *parsed;
                }
            }
            views.push_back(entity::ai::PlayerView{.entityId = player->entityId(),
                                                   .uuid = player->uuid().toString(),
                                                   .x = player->x,
                                                   .y = player->y,
                                                   .z = player->z,
                                                   .width = playerWidth,
                                                   .height = playerHeight,
                                                   .targetable = isTargetable(*player),
                                                   .heldItem = std::move(heldItem),
                                                   .sneaking = player->sneaking,
                                                   .yaw = player->yaw,
                                                   .pitch = player->pitch});
        }
    }

    random::RandomSource random = random::RandomSource::legacy(scratchSeed());
    // Worked out on the first entity and not before, so a world with nothing alive never builds it.
    // The plain radius here: entities stop one chunk sooner than blocks (`ChunkLevel.byStatus`).
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
        // parity: `Mob.aiStep` stops the moment `isDeadOrDying`, so counting to twenty is the whole
        // of a corpse's turn and that already happened above.
        if (entity->dying) {
            continue;
        }
        // An entity is ticked by whoever owns the ground it is standing on. It may walk out of this
        // region during its own turn: that is fine, the next tick asks again, and the repartition
        // that could change the answer only happens between ticks.
        if (!region.ownsPosition(entity->x, entity->z)) {
            continue;
        }
        // parity: `ServerLevel` ticks an entity only when it is a player or its chunk is
        // `inEntityTickingRange`; everything else stays on the list, keeps its box and is still read
        // by whatever stands under it, but takes no turn. Ours ticked every entity in the world, so
        // a minecart three chunks from spawn kept rolling with nobody connected (`vagonetasinticket`).
        if (!ticking(entity->x, entity->z)) {
            continue;
        }
        // parity: `LightningBolt.tick` does everything on the turn its life is still two: it powers
        // the rod under it, lights the ground on normal and hard, and is heard as fourteen. Nothing
        // of it is saved, so the bench sees the rod and never the bolt. Measured in `pararrayos`,
        // where a summoned bolt did nothing at all here.
        if (entity->type() == world::lightning::bolt()) {
            entity->removed = true;
            // parity: `getStrikePosition`, which nudges down by a millionth so a bolt standing on a
            // whole number hits the block under its feet and not the air it is in.
            const auto strikeX = static_cast<std::int32_t>(std::floor(entity->x));
            const auto strikeY = static_cast<std::int32_t>(std::floor(entity->y - 1.0e-6));
            const auto strikeZ = static_cast<std::int32_t>(std::floor(entity->z));
            std::vector<world::BlockChange> struck;
            const bool lightFires = world.difficulty() == "normal" || world.difficulty() == "hard";
            world::lightning::strike(world.state(), strikeX, strikeY, strikeZ, lightFires, random, struck);
            world::vibrations::emit(world.state(), "lightning_strike", entity->x, entity->y, entity->z,
                                    world.gameTime());
            for (const world::BlockChange& change : struck) {
                broadcaster_->announceRedstone(world, watchers, change);
            }
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
            world::vibrations::emit(world.state(), "explode", entity->x, entity->y, entity->z, world.gameTime());
            continue;
        }
        const entity::projectiles::Kind* projectile = entity::projectiles::kindOf(entity->type());
        if (projectile != nullptr) {
            combat_->tickProjectile(world, watchers, living, *entity, *projectile);
            continue;
        }
        // parity: `AbstractMinecart.tick`: on a rail it follows the track, and off one it falls.
        if (world::detectorRails::isMinecart(entity->type())) {
            // parity: `AbstractMinecart.tick`, where what a cart has taken heals a point a tick.
            if (entity->vehicleDamage > 0.0F) {
                entity->vehicleDamage -= 1.0F;
            }
            player::PlayerSession* rider = nullptr;
            for (player::PlayerSession* player : watchers) {
                if (std::ranges::find(entity->passengerIds, player->entityId()) != entity->passengerIds.end()) {
                    rider = player;
                    break;
                }
            }
            // parity: `AbstractMinecart.push`, the shove a cart takes from anybody stood in it.
            for (player::PlayerSession* player : watchers) {
                if (rider != nullptr && player->entityId() == rider->entityId()) {
                    continue;
                }
                if (MobActionSystem::touchesCart(*entity, *player)) {
                    entity::minecarts::pushedBy(*entity, player->x, player->z);
                }
            }
            const std::array<double, 2> intent =
                rider == nullptr ? std::array<double, 2>{0.0, 0.0}
                                 : entity::minecarts::intentOf(rider->inputForward, rider->inputBack, rider->inputLeft,
                                                               rider->inputRight, rider->yaw);
            if (!entity::minecarts::moveAlongTrack(world.state(), *entity, intent[0], intent[1], rider != nullptr)) {
                entity::minecarts::comeOffTrack(world.state(), *entity, RedstoneTickSystem::kMinecartGravity);
            }
            // The rider's own copy of where they are comes from the cart: the client draws itself on
            // the cart, and the server needs the same answer to load chunks around it.
            if (rider != nullptr) {
                rider->x = entity->x;
                rider->y = entity->y;
                rider->z = entity->z;
            }
            // parity: the activator rail, which tells the cart it is on every turn it is over one:
            // a hopper cart stops while the rail is powered and a tnt cart lights its fuse.
            entity::minecarts::activateOnRail(world.state(), *entity,
                                              world.gameRules().get(world::gameRules::kTntExplodes));
            // parity: `MinecartTNT.tick`, which counts the fuse down after the movement and blows up
            // on the turn it reaches zero. The power is vanilla's own base plus what the speed adds.
            if (entity->type().path() == "tnt_minecart" && entity->fuse >= 0) {
                if (entity->fuse > 0) {
                    --entity->fuse;
                } else {
                    entity->removed = true;
                    if (world.gameRules().get(world::gameRules::kTntExplodes)) {
                        const double speed = std::min(std::sqrt((entity->velocityX * entity->velocityX) +
                                                                (entity->velocityZ * entity->velocityZ)),
                                                      entity::minecarts::kTntCartSpeedCap);
                        const double power = entity::minecarts::kTntCartPower + (random.nextDouble() * 1.5 * speed);
                        explosions_->blowUp(world, watchers, entity->x, entity->y, entity->z, static_cast<float>(power),
                                            "tnt", entity->entityId(), world::explosions::sparesRails);
                        world::vibrations::emit(world.state(), "explode", entity->x, entity->y, entity->z,
                                                world.gameTime());
                    }
                    continue;
                }
            }
            // parity: `MinecartHopper.tick` sucks after the movement, and with no cooldown at all:
            // a cart parked under a chest takes one item every tick it is enabled. It is no block
            // entity, so nothing it takes tells a comparator; the container it took from does.
            static_cast<void>(world::hoppers::tickCart(world.state(), *entity, world.gameTime()));
            // parity: a detector rail looks for carts whose box touches its own, so the cart has to
            // ask every rail its box reaches and not only the one under its middle: it is on the
            // next rail a quarter of a block before its centre gets there.
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
        // parity: `Mob.aiStep`, which is what makes an animal wander rather than stand there.
        if (entity::mobs::isMob(entity->type())) {
            mobTicks_->tickTree(world, *entity, watchers, views, living, random);
        } else {
            std::set<std::int32_t> visited;
            mobTicks_->tickPassengers(world, *entity, watchers, views, living, random, visited);
        }
    }
    // What died while the entities were moving dies on this turn and not the next one: an arrow that
    // lands in a pig kills it here, and vanilla's sensor hears that on the very tick.
    tickDeaths(world, region);
}

} // namespace basalt::server::runtime
