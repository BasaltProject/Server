#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockEntityTicks.hpp>
#include <basalt/server/runtime/world/BrewingStands.hpp>
#include <basalt/server/runtime/world/Campfires.hpp>
#include <basalt/server/runtime/world/ComparatorReading.hpp>
#include <basalt/server/runtime/world/Crafters.hpp>
#include <basalt/server/runtime/world/DaylightDetectors.hpp>
#include <basalt/server/runtime/world/Furnaces.hpp>
#include <basalt/server/runtime/world/Geysers.hpp>
#include <basalt/server/runtime/world/Hoppers.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <cstdint>
#include <map>
#include <string>

namespace basalt::server::runtime::world::blockEntityTicks {
namespace {

namespace crafting = server::world::crafting;

/// What a cooker is called in an event, which is the lower-case form of its own name.
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

/// parity: the `state.setValue(LIT, isLit)` at the end of `serverTick`: the block itself carries
/// whether the fire is burning, which is what the client draws and what light comes off it.
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

/// The slot's item, or nothing, for the event that reports what a furnace is working on.
[[nodiscard]] const server::world::loot::ItemStack* slotOf(const furnaces::State& state, std::size_t slot) {
    return slot < state.items.size() && state.items[slot].has_value() ? &*state.items[slot] : nullptr;
}

} // namespace

std::vector<BlockChange> tick(WorldState& world, std::string_view worldName, const WorldClock& clock,
                              const WorldWeather& weather, const std::int64_t gameTime,
                              const std::vector<std::array<std::int32_t, 2>>& chunks, random::RandomSource& random,
                              const Hooks& hooks, bool hasSkylight, std::int64_t worldSeed) {
    std::vector<BlockChange> changes;
    // parity: `DaylightDetectorBlock`'s own ticker, which looks once every twenty ticks.
    // parity: `DaylightDetectorBlock.tickEntity` is `level.getGameTime() % 20L == 0L`, the world's
    // **age**, not its time of day. The two are different counters in the jar and only the second is
    // what `/time set` moves, so reading the clock here put the detector's twenty ticks on the wrong
    // beat after any time change, and froze it altogether wherever the day is not advancing.
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
                    daylightDetectors::update(world, clock, weather, x, entity.y(), z, hasSkylight);
                if (updated.has_value()) {
                    changes.push_back(*updated);
                }
            }
        }
    }
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
            // parity: `SculkSensorBlockEntity`'s ticker, which is `VibrationSystem.Ticker.tick`: a
            // noise on its way moves a block closer every turn of the world and books nothing.
            if (path == "sculk_sensor" || path == "calibrated_sculk_sensor") {
                if (const std::optional<BlockChange> heard = vibrations::travelTick(world, x, y, z, gameTime);
                    heard.has_value()) {
                    changes.push_back(*heard);
                }
                continue;
            }
            // parity: `SERVER_WAITING_COUNTDOWN_TICKER`, the wait a periodic geyser keeps between
            // going off and lying dormant: counted in the block entity, twenty ticks at a time.
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
                if (jukeboxes::tick(world, x, y, z, hooks.songLength ? hooks.songLength(*song) : 0)) {
                    chunk->markDirty();
                    // parity: `JukeboxBlockEntity.onSongChanged` ends in `updateNeighborsAt`, which
                    // is what takes the signal off whatever the jukebox was feeding.
                    if (hooks.songStopped) {
                        hooks.songStopped(x, y, z);
                    }
                }
                continue;
            }
            // parity: `HopperBlockEntity.pushItemsTick`, one move per turn and then a rest.
            if (path == "hopper") {
                if (hooks.allowHopperMove && !hooks.allowHopperMove(worldName, x, y, z)) {
                    continue;
                }
                const hoppers::Around around{
                    .isBookshelfBook = hooks.isBookshelfBook,
                    .recipeFor = hooks.recipeFor,
                    .changes = &changes,
                    .gameTime = gameTime,
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
            // parity: `CampfireBlockEntity.cookTick` while it burns and `cooldownTick` when it does
            // not: what is on a fire that went out loses its progress two at a time.
            if (campfires::isCampfire(path)) {
                const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
                if (!stateId.has_value()) {
                    continue;
                }
                campfires::State fire = campfires::read(entity->data());
                // parity: `changed` in both halves of the campfire's tick, which is a slot with
                // something in it while it burns and a slot with progress left while it does not.
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
                // parity: `cookTick` and `cooldownTick` both only call `setChanged` when a slot
                // actually moved, so an empty campfire does not rewrite its region file every turn.
                if (moved) {
                    chunk->markDirty();
                    // parity: `setChanged` is also what tells a comparator, and nothing reads a
                    // campfire, but the square beside it may hold one over something else.
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
            // parity: `CrafterBlockEntity.serverTick`, six ticks of animation and nothing else.
            if (path == "crafter") {
                const std::optional<BlockChange> turned = crafters::tick(world, x, y, z, entity->data());
                if (turned.has_value()) {
                    chunk->markDirty();
                    changes.push_back(*turned);
                }
                continue;
            }
            // parity: `BrewingStandBlockEntity.serverTick`, which also shows what it holds on the
            // block itself.
            if (path == "brewing_stand") {
                brewingStands::State stand = brewingStands::read(entity->data());
                if (brewingStands::tick(stand)) {
                    brewingStands::write(stand, entity->data());
                    chunk->markDirty();
                    redstoneComponents::scheduleComparatorsAround(world, worldName, x, y, z,
                                                                  comparatorReading::of(world));
                }
                // parity: the bottle bits are compared **every** turn, outside everything the brew
                // does, so a bottle put into a stand with no fuel in it still shows on the block.
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
            if (hooks.allowSmelt &&
                !hooks.allowSmelt(worldName, x, y, z, nameOf(*cooker), slotOf(state, 0), slotOf(state, 2))) {
                continue;
            }
            const furnaces::Ticked turn =
                furnaces::tick(state, [&hooks, cooker](const server::world::loot::ItemStack& stack) {
                    return hooks.recipeFor ? hooks.recipeFor(*cooker, stack) : nullptr;
                });
            if (!turn.moved) {
                continue;
            }
            furnaces::write(state, entity->data());
            // parity: `serverTick` only calls `setChanged` on its own `changed`, so a furnace merely
            // counting down neither dirties its chunk nor tells the comparator anything.
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
    return changes;
}

} // namespace basalt::server::runtime::world::blockEntityTicks
