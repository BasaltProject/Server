#include <basalt/server/plugins/PluginHookEventTypes.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/config/PurpurRules.hpp>
#include <basalt/server/runtime/plugins/EventSink.hpp>
#include <basalt/server/runtime/plugins/HookEvents.hpp>
#include <basalt/server/runtime/world/Beacons.hpp>
#include <basalt/server/runtime/world/Bells.hpp>
#include <basalt/server/runtime/world/BlockEntityTicks.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/EndGateways.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>
#include <basalt/server/world/WorldHeight.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::blockEntityTicks {
namespace {

namespace crafting = server::world::crafting;

[[nodiscard]] std::string_view nameOf(crafting::Cooker cooker) {
    switch (cooker) {
    case crafting::Cooker::Smelting:
        return "smelting";
    case crafting::Cooker::Blasting:
        return "blasting";
    case crafting::Cooker::Smoking:
        return "smoking";
    case crafting::Cooker::Campfire:
        break;
    }
    return "campfire";
}

/// parity: the three `HAS_BOTTLE` bits a stand shows for the bottles standing in it.
[[nodiscard]] std::optional<std::int32_t> bottleChange(WorldState& world, std::int32_t x, std::int32_t y,
                                                       std::int32_t z,
                                                       const std::array<bool, brewingStands::kBottles>& bottles) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    std::map<std::string, std::string> wanted;
    for (std::size_t slot = 0; slot < bottles.size(); ++slot) {
        wanted["has_bottle_" + std::to_string(slot)] = bottles[slot] ? "true" : "false";
    }
    const std::optional<std::int32_t> turned = registry::blockStateDefinitions::stateIdOf(*block, wanted, *stateId);
    if (!turned.has_value() || *turned == *stateId || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return *turned;
}

/// parity: the `state.setValue(LIT, isLit)` at the end of `serverTick`.
[[nodiscard]] std::optional<std::int32_t> litChange(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                    bool lit) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return std::nullopt;
    }
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*stateId);
    if (block == nullptr) {
        return std::nullopt;
    }
    const std::string wanted = lit ? "true" : "false";
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto current = properties.find("lit");
    if (current != properties.end() && current->second == wanted) {
        return std::nullopt;
    }
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(*block, {{"lit", wanted}}, *stateId);
    if (!turned.has_value() || !world.setBlockState(x, y, z, *turned)) {
        return std::nullopt;
    }
    return *turned;
}

void announceBrewing(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                     const brewingStands::State& before, brewingStands::State& stand) {
    const auto ingredientSlot = static_cast<std::size_t>(brewingStands::kIngredientSlot);
    if (before.brewTime == 0 && stand.brewTime > 0 && stand.items[ingredientSlot].has_value()) {
        world.events().notify(&server::plugins::pluginEventTypes::blockBrewStart,
                              [&](server::plugins::PluginEvent& event) {
                                  plugins::hookEvents::position(event, world.eventWorld(), x, y, z);
                                  event.set("source", stand.items[ingredientSlot]->item.toString());
                                  event.set("total_time", static_cast<std::int64_t>(stand.brewTime));
                              });
        return;
    }
    const auto& wasIngredient = before.items[ingredientSlot];
    const auto& nowIngredient = stand.items[ingredientSlot];
    const bool spent =
        wasIngredient.has_value() && (!nowIngredient.has_value() || nowIngredient->count < wasIngredient->count);
    if (before.brewTime != 1 || stand.brewTime != 0 || !spent) {
        return;
    }
    // A brew that a plugin cancels leaves the bottles and the ingredient where they were, with the heat spent.
    if (!world.events().allowed(
            &server::plugins::pluginEventTypes::blockBrew, [&](server::plugins::PluginEvent& event) {
                std::string results;
                for (std::int32_t slot = 0; slot < brewingStands::kBottles; ++slot) {
                    const auto& bottle = stand.items[static_cast<std::size_t>(slot)];
                    results += (slot == 0 ? "" : "\n") + (bottle.has_value() ? bottle->item.toString() : "");
                }
                plugins::hookEvents::position(event, world.eventWorld(), x, y, z);
                event.set("fuel", static_cast<std::int64_t>(stand.fuel));
                event.set("results", std::move(results));
            })) {
        stand.items = before.items;
        stand.potions = before.potions;
    }
}

} // namespace

std::vector<BlockChange> tick(WorldState& world, std::string_view worldName, const WorldClock& clock,
                              const std::int32_t skyDarken, const std::int64_t gameTime,
                              const std::vector<std::array<std::int32_t, 2>>& chunks, random::RandomSource& random,
                              const Hooks& hooks, bool hasSkylight, bool sunMoves, std::int64_t worldSeed) {
    std::vector<BlockChange> changes;
    // parity: `DaylightDetectorBlock`'s own ticker, which looks once every twenty ticks.
    if (gameTime % daylightDetectors::kIntervalTicks == 0) {
        for (const std::array<std::int32_t, 2>& position : chunks) {
            const server::world::Chunk* chunk = world.chunkIfLoaded(position[0], position[1]);
            if (chunk == nullptr) {
                continue;
            }
            for (const server::world::ChunkBlockEntity& entity : chunk->blockEntities()) {
                if (entity.type().path() != "daylight_detector") {
                    continue;
                }
                const std::int32_t x = (position[0] << 4) + entity.localX();
                const std::int32_t z = (position[1] << 4) + entity.localZ();
                const std::optional<BlockChange> updated =
                    daylightDetectors::update(world, clock, skyDarken, x, entity.y(), z, hasSkylight, sunMoves);
                if (updated.has_value()) {
                    changes.push_back(*updated);
                }
            }
        }
    }
    std::vector<BlockChange> hives;
    std::vector<std::array<std::int32_t, 3>> catalysts;
    for (const std::array<std::int32_t, 2>& position : chunks) {
        server::world::Chunk* chunk = world.chunkIfLoaded(position[0], position[1]);
        if (chunk == nullptr || chunk->blockEntityCount() == 0) {
            continue;
        }
        for (const server::world::ChunkBlockEntity& stored : chunk->blockEntities()) {
            const std::int32_t x = (position[0] << 4) + stored.localX();
            const std::int32_t y = stored.y();
            const std::int32_t z = (position[1] << 4) + stored.localZ();
            const std::string path{stored.type().path()};
            // parity: `SculkSensorBlockEntity`'s ticker, which is `VibrationSystem.Ticker.tick`: a.
            if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
                if (const std::optional<BlockChange> heard = vibrations::travelTick(world, x, y, z, gameTime);
                    heard.has_value()) {
                    changes.push_back(*heard);
                }
                continue;
            }
            // parity: `SculkShriekerBlockEntity`'s ticker, the same countdown. The shriek itself.
            if (path == "sculk_shrieker") {
                if (std::optional<vibrations::ShriekerArrival> arrival =
                        vibrations::shriekerTravelTick(world, x, y, z, gameTime, hooks.ticking);
                    arrival.has_value() && hooks.shriekerHeard) {
                    hooks.shriekerHeard(*arrival);
                }
                continue;
            }
            // parity: `SERVER_WAITING_COUNTDOWN_TICKER`, the wait a periodic geyser keeps between.
            if (path == "potent_sulfur") {
                if (const std::optional<BlockChange> flipped =
                        geysers::countdownTick(world, worldSeed, gameTime, x, y, z);
                    flipped.has_value()) {
                    changes.push_back(*flipped);
                }
                continue;
            }
            // parity: `JukeboxBlock.getTicker`, which only runs while there is a disc in.
            if (path == "jukebox") {
                const std::optional<std::string> song = jukeboxes::songAt(world, x, y, z);
                if (!song.has_value()) {
                    continue;
                }
                if (jukeboxes::tick(world, x, y, z, hooks.songLength ? hooks.songLength(*song) : 0, gameTime)) {
                    chunk->markDirty();
                    // parity: `JukeboxBlockEntity.onSongChanged` ends in `updateNeighborsAt`, which.
                    if (hooks.songStopped) {
                        hooks.songStopped(x, y, z);
                    }
                }
                continue;
            }
            if (path == "beehive") {
                if (hooks.beehive && hooks.beehive(x, y, z, hives)) {
                    chunk->markDirty();
                }
                continue;
            }
            if (path == "sculk_catalyst") {
                catalysts.push_back({x, y, z});
                continue;
            }
            if (path == "trial_spawner") {
                if (hooks.trialSpawner && hooks.trialSpawner(x, y, z)) {
                    chunk->markDirty();
                }
                continue;
            }
            if (path == "bell") {
                server::world::ChunkBlockEntity* bellEntity = chunk->blockEntityAt(stored.localX(), y, stored.localZ());
                if (bellEntity != nullptr && bells::ringing(bellEntity->data()) && hooks.bell &&
                    hooks.bell(bellEntity->data(), x, y, z)) {
                    chunk->markDirty();
                }
                continue;
            }
            if (path == "creaking_heart") {
                if (hooks.creakingHeart && hooks.creakingHeart(x, y, z, hives)) {
                    chunk->markDirty();
                }
                continue;
            }
            // parity: `HopperBlockEntity.pushItemsTick`, one move per turn and then a rest.
            if (path == "hopper") {
                if (gameTime % hooks.hopperCheckTicks != 0) {
                    continue;
                }
                if (hooks.allowHopperMove && !hooks.allowHopperMove(worldName, x, y, z)) {
                    continue;
                }
                const hoppers::Around around{
                    .isBookshelfBook = hooks.isBookshelfBook,
                    .doesNotBlockHoppers = hooks.doesNotBlockHoppers,
                    .recipeFor = hooks.recipeFor,
                    .changes = &changes,
                    .gameTime = gameTime,
                    .transferTicks = hooks.hopperTransferTicks,
                    .amount = hooks.hopperAmount,
                };
                if (hoppers::tick(world, x, y, z, random, around)) {
                    chunk->markDirty();
                }
                continue;
            }
            server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(stored.localX(), y, stored.localZ());
            if (entity == nullptr) {
                continue;
            }
            // parity: `TheEndGatewayBlockEntity.portalTick`, one year older every turn, and the beam.
            if (path == "end_gateway") {
                const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
                if (!stateId.has_value() || !endGateways::isGateway(*stateId)) {
                    continue;
                }
                const std::int64_t age = entity->data().getLong("Age", 0) + 1;
                entity->data().putLong("Age", age);
                if (age % endGateways::kAttentionIntervalTicks == 0) {
                    // parity: `triggerCooldown`'s `setChanged` along with the block event.
                    chunk->markDirty();
                    if (hooks.gatewayBeam) {
                        hooks.gatewayBeam(x, y, z);
                    }
                }
                continue;
            }
            // parity: `ConduitBlockEntity.serverTick`.
            if (path == "conduit") {
                if (hooks.conduit && hooks.conduit(entity->data(), x, y, z)) {
                    chunk->markDirty();
                }
                continue;
            }
            // parity: `SpawnerBlock.getTicker`.
            if (path == "mob_spawner") {
                const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
                const wire::common::Identifier* block =
                    stateId.has_value() ? registry::blockStateDefinitions::blockOf(*stateId) : nullptr;
                if (block != nullptr && block->path() == "spawner" && hooks.spawnerTick &&
                    hooks.spawnerTick(entity->data(), x, y, z)) {
                    chunk->markDirty();
                }
                continue;
            }
            // parity: `CampfireBlockEntity.cookTick` while it burns and `cooldownTick` when it does.
            if (campfires::isCampfire(path)) {
                const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
                if (!stateId.has_value()) {
                    continue;
                }
                campfires::State fire = campfires::read(entity->data());
                // parity: `changed` in both halves of the campfire's tick, which is a slot with.
                const bool lit = campfires::isLitCampfire(*stateId);
                bool moved = false;
                for (std::size_t slot = 0; slot < fire.items.size(); ++slot) {
                    moved = moved || (lit ? fire.items[slot].has_value() : fire.progress[slot] > 0);
                }
                const std::vector<server::world::loot::ItemStack> done =
                    campfires::tick(fire, lit, [&hooks](const server::world::loot::ItemStack& stack) {
                        return hooks.recipeFor ? hooks.recipeFor(crafting::Cooker::Campfire, stack) : nullptr;
                    });
                campfires::write(fire, entity->data());
                // parity: `cookTick` and `cooldownTick` both only call `setChanged` when a slot.
                if (moved) {
                    chunk->markDirty();
                    // parity: `setChanged` is also what tells a comparator, and nothing reads a.
                    redstoneComponents::scheduleComparatorsAround(world, worldName, x, y, z,
                                                                  comparatorReading::of(world));
                }
                for (const server::world::loot::ItemStack& result : done) {
                    if (hooks.onDropped) {
                        hooks.onDropped(Dropped{.x = x, .y = y, .z = z, .stack = result});
                    }
                }
                continue;
            }
            // parity: `BeaconBlockEntity.tick`, which counts its pyramid and hands out its effects.
            if (path == "beacon") {
                if (gameTime % beacons::kApplyInterval != 0) {
                    continue;
                }
                beacons::Stored beacon = beacons::read(entity->data());
                // parity: `BeaconBlockEntity.tick` walks the column above for blocks light goes.
                const auto passes = [&world](const std::int32_t bx, const std::int32_t by, const std::int32_t bz) {
                    const std::optional<std::int32_t> state = world.blockState(bx, by, bz);
                    if (!state.has_value()) {
                        return false;
                    }
                    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(*state);
                    return block != nullptr &&
                           (beacons::letsBeamThrough(registry::blockShapes::dampening(*state), block->path()) ||
                            (world.gameplay().blocks.beaconAllowEffectsWithTintedGlass &&
                             block->path() == "tinted_glass"));
                };
                // parity: with the beam blocked the jar leaves the count alone and hands out nothing.
                if (!beacons::beamReachesSky(passes, x, y, z, server::world::worldHeight::kMaxY)) {
                    continue;
                }
                const std::int32_t levels = beacons::levelsUnder(
                    [&world, &hooks](const std::int32_t bx, const std::int32_t by, const std::int32_t bz) {
                        const std::optional<std::int32_t> state = world.blockState(bx, by, bz);
                        const wire::common::Identifier* block =
                            state.has_value() ? registry::blockStateDefinitions::blockOf(*state) : nullptr;
                        return block != nullptr && hooks.isBeaconBase && hooks.isBeaconBase(*block);
                    },
                    x, y, z, server::world::worldHeight::kMinY);
                const bool moved = levels != beacon.levels;
                // parity: the criterion is raised on the dark-to-lit edge only, before the new count.
                if (beacon.levels <= 0 && levels > 0 && hooks.beaconLit) {
                    hooks.beaconLit(x, y, z, levels);
                }
                beacon.levels = levels;
                beacons::write(beacon, entity->data());
                if (moved) {
                    chunk->markDirty();
                }
                if (levels > 0 && !beacon.primary.empty() && hooks.beaconPulse) {
                    hooks.beaconPulse(x, y, z, levels, beacon.primary, beacon.secondary);
                }
                continue;
            }
            // parity: `CrafterBlockEntity.serverTick`, six ticks of animation and nothing else.
            if (path == "crafter") {
                const std::optional<BlockChange> turned = crafters::tick(world, x, y, z, entity->data());
                if (turned.has_value()) {
                    chunk->markDirty();
                    changes.push_back(*turned);
                }
                continue;
            }
            // parity: `BrewingStandBlockEntity.serverTick`, which also shows what it holds on the.
            if (path == "brewing_stand") {
                brewingStands::State stand = brewingStands::read(entity->data());
                const bool watching = world.events().listening(&server::plugins::pluginEventTypes::blockBrewStart) ||
                                      world.events().listening(&server::plugins::pluginEventTypes::blockBrew);
                std::optional<brewingStands::State> before;
                if (watching) {
                    before = stand;
                }
                if (brewingStands::tick(stand, [&hooks, x, y, z](const server::world::loot::ItemStack& remainder) {
                        if (hooks.onDropped) {
                            hooks.onDropped(Dropped{.x = x, .y = y, .z = z, .stack = remainder});
                        }
                    })) {
                    if (before.has_value()) {
                        announceBrewing(world, x, y, z, *before, stand);
                    }
                    brewingStands::write(stand, entity->data());
                    chunk->markDirty();
                    redstoneComponents::scheduleComparatorsAround(world, worldName, x, y, z,
                                                                  comparatorReading::of(world));
                }
                // parity: the bottle bits are compared **every** turn, outside everything the brew.
                const std::optional<std::int32_t> turned =
                    bottleChange(world, x, y, z, brewingStands::bottleBits(stand));
                if (turned.has_value()) {
                    changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned});
                }
                continue;
            }
            const std::optional<crafting::Cooker> cooker = furnaces::cookerFor(path);
            if (!cooker.has_value()) {
                continue;
            }
            furnaces::State state = furnaces::read(entity->data());
            furnaces::Notify notify;
            if (hooks.furnaceBurn) {
                notify.burn = [&hooks, &worldName, &cooker, x, y, z](const server::world::loot::ItemStack& fuel,
                                                                     std::int32_t& burnTicks) {
                    return hooks.furnaceBurn(worldName, x, y, z, nameOf(*cooker), fuel, burnTicks);
                };
            }
            if (hooks.furnaceStarted) {
                notify.started = [&hooks, &worldName, &cooker, x, y, z](const server::world::loot::ItemStack& source,
                                                                        std::int32_t total, std::string_view recipe) {
                    hooks.furnaceStarted(worldName, x, y, z, nameOf(*cooker), source, total, recipe);
                };
            }
            if (hooks.allowSmelt) {
                notify.smelt = [&hooks, &worldName, &cooker, x, y, z](const server::world::loot::ItemStack& source,
                                                                      server::world::loot::ItemStack& result,
                                                                      std::string_view recipe) {
                    return hooks.allowSmelt(worldName, x, y, z, nameOf(*cooker), source, result, recipe);
                };
            }
            const furnaces::Ticked turn = furnaces::tick(
                state,
                [&hooks, cooker](const server::world::loot::ItemStack& stack) {
                    return hooks.recipeFor ? hooks.recipeFor(*cooker, stack) : nullptr;
                },
                furnaces::kDefaultStackSize, *cooker,
                [&hooks, x, y, z](const server::world::loot::ItemStack& remainder) {
                    if (hooks.onDropped) {
                        hooks.onDropped(Dropped{.x = x, .y = y, .z = z, .stack = remainder});
                    }
                },
                notify);
            if (!turn.moved) {
                continue;
            }
            furnaces::write(state, entity->data());
            // parity: `serverTick` only calls `setChanged` on its own `changed`, so a furnace merely.
            if (turn.changed) {
                chunk->markDirty();
                redstoneComponents::scheduleComparatorsAround(world, worldName, x, y, z, comparatorReading::of(world));
            }
            const std::optional<std::int32_t> turned = litChange(world, x, y, z, state.lit());
            if (turned.has_value()) {
                changes.push_back(BlockChange{.x = x, .y = y, .z = z, .stateId = *turned});
            }
        }
    }
    // parity: the honey a bee brought in, which the jar sets inside the hive's own turn.
    for (BlockChange& honey : hives) {
        if (world.setBlockState(honey.x, honey.y, honey.z, honey.stateId)) {
            changes.push_back(std::move(honey));
        }
    }
    if (hooks.sculkCatalyst) {
        for (const auto& [x, y, z] : catalysts) {
            static_cast<void>(hooks.sculkCatalyst(x, y, z));
        }
    }
    return changes;
}

} // namespace basalt::server::runtime::world::blockEntityTicks
