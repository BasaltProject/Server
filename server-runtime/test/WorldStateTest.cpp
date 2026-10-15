#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockInventory.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
namespace sched = basalt::server::runtime::sched;
namespace entity = basalt::server::runtime::entity;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
using basalt::wire::common::Uuid;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

/// A world whose chunks are made of nothing, which is what a test wants: the real generator is a
/// whole chunk of noise per call.
[[nodiscard]] WorldState emptyWorld(std::int32_t* generated = nullptr) {
    return WorldState{{}, [generated](std::int32_t chunkX, std::int32_t chunkZ) {
                          if (generated != nullptr) {
                              ++*generated;
                          }
                          return Chunk{chunkX, chunkZ};
                      }};
}

/// A light pass that computes nothing: what is under test is which chunks keep theirs, not the
/// light itself.
const std::function<Chunk::PackedLight()> emptyLight = [] { return Chunk::PackedLight{}; };

} // namespace

TEST_CASE("a chunk is generated once and then answered from memory") {
    std::int32_t generated = 0;
    WorldState world = emptyWorld(&generated);
    CHECK(world.loadedChunkCount() == 0);
    CHECK_FALSE(world.isLoaded(0, 0));
    CHECK(world.chunkIfLoaded(0, 0) == nullptr);
    CHECK(world.chunkSetVersion() == 0);

    const Chunk& first = world.getOrGenerateChunk(0, 0);
    CHECK(generated == 1);
    CHECK(world.isLoaded(0, 0));
    CHECK(world.loadedChunkCount() == 1);
    CHECK(world.chunkSetVersion() == 1);

    // The second call is the same chunk, not a second one: the world is what the players edited.
    const Chunk& again = world.getOrGenerateChunk(0, 0);
    CHECK(&again == &first);
    CHECK(generated == 1);
    CHECK(world.chunkSetVersion() == 1);

    // And a detached read of a chunk already in the world answers nothing, so the caller reads it.
    CHECK_FALSE(world.generateDetached(0, 0).has_value());
    CHECK(world.generateDetached(1, 0).has_value());
}

TEST_CASE("a chunk west of nothing is not the chunk north of it") {
    // The packed key masks the low half: without it every negative x lands on the same key as the
    // negative z beside it, and half the world is one chunk.
    CHECK(WorldState::key(-1, 0) != WorldState::key(-1, -1));
    CHECK(WorldState::key(-1, 0) != WorldState::key(0, -1));
    CHECK(WorldState::key(-1, -1) != WorldState::key(0, 0));

    WorldState world = emptyWorld();
    world.getOrGenerateChunk(-1, 0);
    world.getOrGenerateChunk(-1, -1);
    CHECK(world.loadedChunkCount() == 2);
    CHECK(world.chunkIfLoaded(-1, 0)->chunkZ() == 0);
    CHECK(world.chunkIfLoaded(-1, -1)->chunkZ() == -1);
}

TEST_CASE("two workers racing on the same chunk install one of them") {
    WorldState world = emptyWorld();
    std::optional<Chunk> first = world.generateDetached(3, 4);
    std::optional<Chunk> second = world.generateDetached(3, 4);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());

    Chunk& installed = world.install(std::move(*first));
    // The second one arrives to find the position taken, and the world hands back what is there
    // rather than replacing it under whoever is already reading it.
    Chunk& raced = world.install(std::move(*second));
    CHECK(&installed == &raced);
    CHECK(world.loadedChunkCount() == 1);
    CHECK(world.chunkSetVersion() == 1);
}

TEST_CASE("blocks are read and written through the chunk they are in") {
    WorldState world = emptyWorld();
    // Nothing is loaded, so nothing is there and nothing can be written.
    CHECK_FALSE(world.blockState(0, 64, 0).has_value());
    CHECK_FALSE(world.setBlockState(0, 64, 0, stateOf("stone")));

    world.getOrGenerateChunk(0, 0);
    CHECK(world.blockState(0, 64, 0) == registry::blocks::air().stateId);
    CHECK(world.setBlockState(0, 64, 0, stateOf("stone")));
    CHECK(world.blockState(0, 64, 0) == stateOf("stone"));

    // Coordinates are world coordinates: the chunk to the east is a different chunk.
    CHECK_FALSE(world.blockState(16, 64, 0).has_value());
    world.getOrGenerateChunk(1, 0);
    CHECK(world.setBlockState(16, 64, 0, stateOf("stone")));
    CHECK(world.blockState(16, 64, 0) == stateOf("stone"));
    CHECK(world.chunkIfLoaded(1, 0)->blockStateId(0, 64, 0) == stateOf("stone"));

    // Above the sky and below the bedrock are not blocks, which is what something that fell out of
    // the world asks about.
    CHECK_FALSE(world.blockState(0, 5000, 0).has_value());
    CHECK_FALSE(world.blockState(0, -5000, 0).has_value());
    // parity: `Level.setBlock` answers no rather than throwing, and the throw would come out on
    // the tick thread.
    CHECK_FALSE(world.setBlockState(0, 5000, 0, stateOf("stone")));
    CHECK_FALSE(world.setBlockState(0, -5000, 0, stateOf("stone")));
}

TEST_CASE("a write that leaves a block with an entity creates the entity, whatever the write was") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const Chunk* chunk = world.chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);

    // parity: `LevelChunk.setBlockState` gives a state that owns a block entity one whenever none
    // is there, with no say from the flags of the write.
    CHECK(world.setBlockState(8, 64, 8, stateOf("chest")));
    const basalt::server::world::ChunkBlockEntity* chest = chunk->blockEntityAt(8, 64, 8);
    REQUIRE(chest != nullptr);
    CHECK(chest->type() == Identifier::vanilla("chest"));

    // A rewrite of the same block keeps the entity it already has.
    const std::optional<std::int32_t> turned =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla("chest"), {{"type", "left"}}, stateOf("chest"));
    REQUIRE(turned.has_value());
    CHECK(world.setBlockState(8, 64, 8, *turned));
    CHECK(chunk->blockEntityAt(8, 64, 8) == chest);

    // A block change replaces the entity, like the jar removing the old one and making a fresh one.
    CHECK(world.setBlockState(8, 64, 8, stateOf("furnace")));
    const basalt::server::world::ChunkBlockEntity* furnace = chunk->blockEntityAt(8, 64, 8);
    REQUIRE(furnace != nullptr);
    CHECK(furnace->type() == Identifier::vanilla("furnace"));

    // `moving_piston` is the one the jar never leaves default: the piston writes the carrying
    // entity itself in the same breath.
    const std::optional<std::int32_t> moving = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("moving_piston"), {{"facing", "north"}, {"type", "normal"}}, std::nullopt);
    REQUIRE(moving.has_value());
    CHECK(world.setBlockState(9, 64, 8, *moving));
    CHECK(chunk->blockEntityAt(9, 64, 8) == nullptr);

    // And a block that owns nothing leaves nothing behind.
    CHECK(world.setBlockState(8, 64, 8, stateOf("stone")));
    CHECK(chunk->blockEntityAt(8, 64, 8) == nullptr);
}

TEST_CASE("a block that changed makes its neighbours light themselves again") {
    WorldState world = emptyWorld();
    for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            world.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    // Light every chunk, so the invalidation has something to throw away.
    for (Chunk* chunk : world.chunks()) {
        static_cast<void>(chunk->skyLight(emptyLight));
        CHECK(chunk->cachedSkyLight() != nullptr);
    }

    // A torch or a hole in the roof reaches across the border, so every neighbour that could have
    // read the old block is discarded with it.
    CHECK(world.setBlockState(0, 64, 0, stateOf("stone")));
    for (Chunk* chunk : world.chunks()) {
        CAPTURE(chunk->chunkX());
        CAPTURE(chunk->chunkZ());
        CHECK(chunk->cachedSkyLight() == nullptr);
    }
}

TEST_CASE("a chunk arriving relights the neighbours it now shades") {
    WorldState world = emptyWorld();
    Chunk& west = world.getOrGenerateChunk(-1, 0);
    static_cast<void>(west.skyLight(emptyLight));
    CHECK(west.cachedSkyLight() != nullptr);

    // parity: vanilla relights a chunk's neighbourhood on load. Without it the neighbours keep the
    // light they had when this chunk was still empty sky.
    world.getOrGenerateChunk(0, 0);
    CHECK(west.cachedSkyLight() == nullptr);
}

TEST_CASE("what is saved and what is evicted") {
    WorldState world = emptyWorld();
    Chunk& here = world.getOrGenerateChunk(0, 0);
    Chunk& east = world.getOrGenerateChunk(1, 0);
    CHECK(world.dirtyChunks().empty());

    world.setBlockState(0, 64, 0, stateOf("stone"));
    REQUIRE(world.dirtyChunks().size() == 1);
    CHECK(world.dirtyChunks()[0] == &here);

    // A chunk with unsaved changes is not evicted, however far away it is: that is somebody's
    // building.
    CHECK(world.evictClean({&here, &east}) == 1);
    CHECK(world.isLoaded(0, 0));
    CHECK_FALSE(world.isLoaded(1, 0));
    CHECK(world.chunkSetVersion() == 3);

    // Once it is written out it goes like any other.
    here.markClean();
    CHECK(world.dirtyChunks().empty());
    CHECK(world.evictClean({&here}) == 1);
    CHECK(world.loadedChunkCount() == 0);

    // Evicting what is not there is not an error.
    CHECK(world.evictClean({}) == 0);
}

TEST_CASE("which chunks a caller is missing, and which it wants to keep") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    world.getOrGenerateChunk(1, 0);

    CHECK(world.countMissing({{0, 0}, {1, 0}}) == 0);
    CHECK(world.countMissing({{0, 0}, {5, 5}, {6, 6}}) == 2);
    CHECK(world.countMissing({}) == 0);

    // Everything the predicate rejects, which is what a view-distance sweep asks.
    const std::vector<Chunk*> candidates =
        world.evictionCandidates([](std::int32_t chunkX, std::int32_t) { return chunkX == 0; });
    REQUIRE(candidates.size() == 1);
    CHECK(candidates[0]->chunkX() == 1);

    const std::vector<std::int64_t> keys = world.loadedChunkKeys();
    CHECK(keys.size() == 2);
    CHECK(std::ranges::find(keys, WorldState::key(1, 0)) != keys.end());
}

TEST_CASE("a scheduled tick comes due where it was scheduled") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const Identifier water = Identifier::vanilla("water");

    // Nothing is scheduled where no chunk is loaded, and nothing is thrown either.
    world.scheduleBlockTick(500, 64, 500, water, 2);
    world.scheduleBlockTick(0, 5000, 0, water, 2);
    CHECK(world.dueBlockTicks().empty());

    world.scheduleBlockTick(3, 64, 5, water, 2);
    CHECK(world.hasScheduledBlockTick(3, 64, 5, water));
    CHECK_FALSE(world.hasScheduledBlockTick(3, 64, 6, water));
    CHECK_FALSE(world.hasScheduledBlockTick(3, 64, 5, Identifier::vanilla("lava")));
    CHECK_FALSE(world.hasScheduledBlockTick(500, 64, 500, water));

    // A delay of two fires on the second turn after it was booked, not the third. parity: a turn
    // counts down once when it opens, so the turn that books a tick never shortens it.
    world.openTurn();
    CHECK(world.dueBlockTicks().empty());
    world.openTurn();
    const std::vector<std::array<std::int32_t, 3>> due = world.dueBlockTicks();
    REQUIRE(due.size() == 1);
    // World coordinates, not the chunk's own.
    CHECK(due[0] == std::array<std::int32_t, 3>{3, 64, 5});
    CHECK(world.dueBlockTicks().empty());

    // Fluid ticks are their own queue.
    world.scheduleFluidTick(3, 64, 5, water, 0);
    CHECK(world.dueBlockTicks().empty());
    CHECK(world.dueFluidTicks().size() == 1);
}

TEST_CASE("a tick scheduled in a far chunk carries that chunk's coordinates") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(-2, 3);
    world.scheduleBlockTick(-17, 64, 50, Identifier::vanilla("water"), 0);
    const std::vector<std::array<std::int32_t, 3>> due = world.dueBlockTicks();
    REQUIRE(due.size() == 1);
    CHECK(due[0] == std::array<std::int32_t, 3>{-17, 64, 50});
}

TEST_CASE("the world answers the collision code the way it asks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    world.setBlockState(1, 64, 1, stateOf("stone"));

    const entity::entityCollision::BlockAt blockAt = world.blockAccessor();
    CHECK(blockAt(1, 64, 1) == stateOf("stone"));
    CHECK(blockAt(2, 64, 1) == registry::blocks::air().stateId);
    // An unloaded chunk answers nothing, which is what tells the collision code not to stop there.
    CHECK_FALSE(blockAt(500, 64, 500).has_value());
}

TEST_CASE("what the world holds besides blocks") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);

    // Entities, items and lit tnt all go through the world and come back out of it.
    world.spawnEntity(
        std::make_unique<entity::ServerEntity>(1, Uuid{1, 1}, Identifier::vanilla("cow"), 0, 0.5, 64.0, 0.5));
    CHECK(world.entities().size() == 1);
    CHECK(world.entity(1) != nullptr);
    CHECK(world.entitiesIn(0, 0).size() == 1);
    CHECK(world.chunksWithEntities().size() == 1);

    world.spawnItem(DroppedItem{.entityId = 2,
                                .uuid = Uuid{1, 2},
                                .item = Identifier::vanilla("stone"),
                                .count = 1,
                                .x = 0.5,
                                .y = 64.0,
                                .z = 0.5});
    CHECK(world.items().size() == 1);
    CHECK(world.nearbyItems(0.0, 64.0, 0.0, 5.0).size() == 1);
    static_cast<void>(world.tickItems());
    CHECK(world.items().size() == 1);
    CHECK(world.removeItem(2).has_value());
    CHECK(world.items().empty());

    world.primeTnt(1, 64, 1, "prime_fuse");
    CHECK(world.takePrimedTnt().size() == 1);
    CHECK(world.takePrimedTnt().empty());

    world.entity(1)->removed = true;
    CHECK(world.collectRemovedEntities() == std::vector<std::int32_t>{1});
    CHECK(world.entities().empty());
}

TEST_CASE("a chunk the world was built with is a chunk of the world") {
    std::vector<Chunk> seeded;
    seeded.emplace_back(0, 0);
    seeded.back().setBlockStateId(1, 64, 1, stateOf("stone"));
    seeded.emplace_back(1, 0);
    WorldState world{std::move(seeded)};

    CHECK(world.loadedChunkCount() == 2);
    CHECK(world.blockState(1, 64, 1) == stateOf("stone"));
    CHECK(world.isLoaded(1, 0));
}

TEST_CASE("only the owner touches the world, and only while somebody is checking") {
    WorldState world = emptyWorld();
    sched::Owner owner{"world"};
    world.setOwner(&owner);
    owner.bindToCurrentThread();

    const bool previous = sched::ownership::isStrict();
    sched::ownership::setStrict(true);
    // The owning thread is free to write.
    CHECK_NOTHROW(world.setBlockState(0, 64, 0, stateOf("stone")));

    std::thread{[&world]() {
        // Anybody else is not, and says which world and what they touched.
        CHECK_THROWS_AS(world.setBlockState(0, 64, 0, 0), std::logic_error);
        // Reads are not owned: a connection thread sends chunks while the tick thread writes them.
        CHECK_NOTHROW(static_cast<void>(world.blockState(0, 64, 0)));
    }}.join();

    // With the world split the check cannot say anything useful, so it says nothing.
    world.setRegionCount([] { return 4; });
    std::thread{[&world]() { CHECK_NOTHROW(world.setBlockState(0, 64, 0, 0)); }}.join();

    sched::ownership::setStrict(previous);
}

TEST_CASE("a chunk installed later is installed by whoever owns the installing") {
    WorldState world = emptyWorld();
    sched::Owner installOwner{"install"};
    world.setInstallOwner(&installOwner);
    installOwner.bindToCurrentThread();

    std::int32_t announced = 0;
    world.setOnChunkInstalled([&announced](Chunk&) { ++announced; });

    std::optional<Chunk> generated = world.generateDetached(2, 2);
    REQUIRE(generated.has_value());
    // The owner is this thread, so it runs on the spot.
    world.installLater(std::move(*generated));
    CHECK(world.isLoaded(2, 2));
    CHECK(announced == 1);

    // From another thread it waits its turn instead.
    std::optional<Chunk> queued = world.generateDetached(3, 3);
    REQUIRE(queued.has_value());
    std::thread{[&world, &queued]() { world.installLater(std::move(*queued)); }}.join();
    CHECK_FALSE(world.isLoaded(3, 3));
    CHECK(installOwner.runPending() == 1);
    CHECK(world.isLoaded(3, 3));
    CHECK(announced == 2);
}

TEST_CASE("a sensor is remembered where it was placed, and forgotten where it was broken") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    CHECK_FALSE(world.hasSculkSensorNear(0.5, 64.0, 0.5, 8));

    world.setBlockState(4, 64, 4, stateOf("sculk_sensor"));
    CHECK(world.hasSculkSensorNear(0.5, 64.0, 0.5, 8));
    // Only what is within range of the vibration, which is what the sensor is asked for.
    CHECK_FALSE(world.hasSculkSensorNear(0.5, 64.0, 0.5, 2));

    world.setBlockState(4, 64, 4, registry::blocks::air().stateId);
    CHECK_FALSE(world.hasSculkSensorNear(0.5, 64.0, 0.5, 8));
}

TEST_CASE("a world nobody owns installs the chunk itself and still says so") {
    WorldState world = emptyWorld();
    std::int32_t announced = 0;
    world.setOnChunkInstalled([&announced](Chunk&) { ++announced; });

    std::optional<Chunk> generated = world.generateDetached(1, 1);
    REQUIRE(generated.has_value());
    world.installLater(std::move(*generated));
    CHECK(world.isLoaded(1, 1));
    // The chunk-load event fires whichever path installed it: a test world and a running one both
    // have plugins listening.
    CHECK(announced == 1);
}

TEST_CASE("what every loaded chunk costs, for the report an operator reads") {
    WorldState world = emptyWorld();
    CHECK(world.chunkMemory().bytes == 0);

    world.getOrGenerateChunk(0, 0);
    world.getOrGenerateChunk(1, 0);
    world.setBlockState(0, 64, 0, stateOf("stone"));
    const WorldState::ChunkMemory memory = world.chunkMemory();
    CHECK(memory.bytes > 0);
    CHECK(memory.blockEntities == 0);
    CHECK(memory.scheduledTicks == 0);

    world.scheduleBlockTick(0, 64, 0, Identifier::vanilla("water"), 5);
    CHECK(world.chunkMemory().scheduledTicks == 1);
}

TEST_CASE("what a container held falls out when its block goes") {
    // parity: `BlockEntity.preRemoveSideEffects`, which **every** block entity that is a container
    // runs when its block is replaced. It is not in the loot table (a chest's is one chest and
    // nothing else) and nothing here did it, so breaking a full chest lost what was in it.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("chest")));
    Chunk* chunk = world.chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    basalt::wire::nbt::NbtCompound data;
    blockInventory::Slots slots(27);
    slots[0] = basalt::server::world::loot::ItemStack{.item = Identifier::vanilla("stone"), .count = 5};
    slots[3] = basalt::server::world::loot::ItemStack{.item = Identifier::vanilla("dirt"), .count = 2};
    blockInventory::write(data, slots);
    chunk->setBlockEntity(basalt::server::world::ChunkBlockEntity{8, 64, 8, Identifier::vanilla("chest"), data});

    // A state change that keeps the block keeps the contents.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("chest")));
    CHECK(world.takeSpilledContents().empty());

    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::air().stateId));

    const std::vector<WorldState::Spilled> spilled = world.takeSpilledContents();
    REQUIRE(spilled.size() == 1);
    CHECK(spilled.front().x == 8);
    CHECK(spilled.front().y == 64);
    CHECK(spilled.front().z == 8);
    REQUIRE(spilled.front().stacks.size() == 2);
    CHECK(spilled.front().stacks[0].item == Identifier::vanilla("stone"));
    CHECK(spilled.front().stacks[0].count == 5);
    CHECK(spilled.front().stacks[1].item == Identifier::vanilla("dirt"));
    // Taken once and gone: the queue is drained by whoever turns it into items.
    CHECK(world.takeSpilledContents().empty());
}

TEST_CASE("a write that skips the block entity side effects spills nothing") {
    // parity: `/setblock` and `/fill` write with `2 | 256`, and 256 is
    // `UPDATE_SKIP_BLOCK_ENTITY_SIDEEFFECTS`, so a chest replaced by command takes its contents with
    // it. Measured on `cofrerompe`, where our side dropped items the jar never spawned.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("chest")));
    Chunk* chunk = world.chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    basalt::wire::nbt::NbtCompound data;
    blockInventory::Slots slots(27);
    slots[0] = basalt::server::world::loot::ItemStack{.item = Identifier::vanilla("stone"), .count = 5};
    blockInventory::write(data, slots);
    chunk->setBlockEntity(basalt::server::world::ChunkBlockEntity{8, 64, 8, Identifier::vanilla("chest"), data});

    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::air().stateId, WorldState::SideEffects::kSkip));

    CHECK(world.takeSpilledContents().empty());
}

TEST_CASE("a shulker box takes what it held with it") {
    // parity: `ShulkerBoxBlockEntity.preRemoveSideEffects` is empty, which is the whole point of the
    // block: the contents ride in the item. Every other container in the game runs the base one.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("shulker_box")));
    Chunk* chunk = world.chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    basalt::wire::nbt::NbtCompound data;
    blockInventory::Slots slots(27);
    slots[0] = basalt::server::world::loot::ItemStack{.item = Identifier::vanilla("stone"), .count = 5};
    blockInventory::write(data, slots);
    chunk->setBlockEntity(basalt::server::world::ChunkBlockEntity{8, 64, 8, Identifier::vanilla("shulker_box"), data});

    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::air().stateId));

    CHECK(world.takeSpilledContents().empty());
}

TEST_CASE("a block with no container behind it spills nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));

    REQUIRE(world.setBlockState(8, 64, 8, registry::blocks::air().stateId));

    CHECK(world.takeSpilledContents().empty());
}
