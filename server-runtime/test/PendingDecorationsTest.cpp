#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PendingChunkDecorations.hpp>

#include <doctest.h>

#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <string>

using namespace basalt::server::runtime::world;
namespace world = basalt::server::world;
namespace registry = basalt::server::registry;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

/// The chunks a test has loaded, answered the way the world answers.
class LoadedChunks {
public:
    world::Chunk& add(std::int32_t chunkX, std::int32_t chunkZ) {
        const std::int64_t key = PendingChunkDecorations::key(chunkX, chunkZ);
        chunks_.emplace(key, std::make_unique<world::Chunk>(chunkX, chunkZ));
        return *chunks_.at(key);
    }

    [[nodiscard]] PendingChunkDecorations::ChunkAt accessor() {
        return [this](std::int64_t key) -> world::Chunk* {
            const auto found = chunks_.find(key);
            return found == chunks_.end() ? nullptr : found->second.get();
        };
    }

private:
    std::map<std::int64_t, std::unique_ptr<world::Chunk>> chunks_;
};

} // namespace

TEST_CASE("a write that crossed a border lands in the chunk it was aimed at") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    LoadedChunks chunks;
    world::Chunk& east = chunks.add(1, 0);

    // Written at x = 16, which is the first column of the chunk to the east.
    const world::ChunkGeneratedBlockWrite write{.x = 16,
                                                .y = 64,
                                                .z = 3,
                                                .expectedStateId = registry::blocks::air().stateId,
                                                .stateId = stateOf("oak_log"),
                                                .replaceAnyState = false,
                                                .policy = world::GeneratedBlockWritePolicy::Exact};
    pending.deliverGeneratedBlockWrites(std::array{write}, chunks.accessor());
    CHECK(east.blockStateId(0, 64, 3) == stateOf("oak_log"));
}

TEST_CASE("a write for a chunk nobody has loaded waits for it") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    LoadedChunks chunks;

    const world::ChunkGeneratedBlockWrite write{
        .x = 16, .y = 64, .z = 3, .expectedStateId = registry::blocks::air().stateId, .stateId = stateOf("oak_log")};
    // Nothing is loaded there yet, so nothing is written and nothing is lost.
    pending.deliverGeneratedBlockWrites(std::array{write}, chunks.accessor());

    world::Chunk& east = chunks.add(1, 0);
    CHECK(east.blockStateId(0, 64, 3) == registry::blocks::air().stateId);
    pending.applyPendingGeneratedBlockWrites(east);
    CHECK(east.blockStateId(0, 64, 3) == stateOf("oak_log"));

    // And it is applied once: the second install of the same chunk finds nothing waiting.
    world::Chunk again{1, 0};
    pending.applyPendingGeneratedBlockWrites(again);
    CHECK(again.blockStateId(0, 64, 3) == registry::blocks::air().stateId);
}

TEST_CASE("a write only lands where the block it expected is still there") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    LoadedChunks chunks;
    world::Chunk& east = chunks.add(1, 0);
    east.setBlockStateId(0, 64, 3, stateOf("stone"));

    // The terrain moved on since the decoration was planned: the write is dropped rather than
    // carving a hole in whatever arrived first.
    const world::ChunkGeneratedBlockWrite exact{
        .x = 16, .y = 64, .z = 3, .expectedStateId = registry::blocks::air().stateId, .stateId = stateOf("oak_log")};
    pending.deliverGeneratedBlockWrites(std::array{exact}, chunks.accessor());
    CHECK(east.blockStateId(0, 64, 3) == stateOf("stone"));

    // Unless it was told to write over anything, which is what a structure does.
    world::ChunkGeneratedBlockWrite anything = exact;
    anything.replaceAnyState = true;
    pending.deliverGeneratedBlockWrites(std::array{anything}, chunks.accessor());
    CHECK(east.blockStateId(0, 64, 3) == stateOf("oak_log"));
}

TEST_CASE("a write outside the chunk's own height is not written at all") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    LoadedChunks chunks;
    world::Chunk& east = chunks.add(1, 0);

    const world::ChunkGeneratedBlockWrite tooHigh{.x = 16,
                                                  .y = 5000,
                                                  .z = 3,
                                                  .expectedStateId = registry::blocks::air().stateId,
                                                  .stateId = stateOf("oak_log"),
                                                  .replaceAnyState = true};
    // Nothing thrown and nothing written: a decoration that overshot the world is not the world's
    // problem to store.
    pending.deliverGeneratedBlockWrites(std::array{tooHigh}, chunks.accessor());
    CHECK(east.blockStateId(0, 64, 3) == registry::blocks::air().stateId);
}

TEST_CASE("a block entity needs the block it belongs to") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    LoadedChunks chunks;
    world::Chunk& east = chunks.add(1, 0);
    east.setBlockStateId(0, 64, 3, stateOf("chest"));

    NbtCompound data;
    data.putString("LootTable", "minecraft:chests/simple_dungeon");
    const world::ChunkGeneratedBlockEntityWrite write{.x = 16,
                                                      .y = 64,
                                                      .z = 3,
                                                      .expectedStateId = stateOf("chest"),
                                                      .type = Identifier::vanilla("chest"),
                                                      .data = data};
    pending.deliverGeneratedBlockEntityWrites(std::array{write}, chunks.accessor());
    CHECK(east.blockEntityAt(0, 64, 3) != nullptr);

    // A chest with no chest under it is worse than no chest: the write is dropped.
    world::Chunk& west = chunks.add(-1, 0);
    world::ChunkGeneratedBlockEntityWrite orphan = write;
    orphan.x = -1;
    pending.deliverGeneratedBlockEntityWrites(std::array{orphan}, chunks.accessor());
    CHECK(west.blockEntityAt(15, 64, 3) == nullptr);
}

TEST_CASE("a scheduled tick crosses the border with the block it belongs to") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    LoadedChunks chunks;
    world::Chunk& east = chunks.add(1, 0);
    east.setBlockStateId(0, 64, 3, stateOf("water"));

    const world::ChunkGeneratedBlockTick tick{.x = 16,
                                              .y = 64,
                                              .z = 3,
                                              .expectedStateId = stateOf("water"),
                                              .type = Identifier::vanilla("water"),
                                              .delay = 5,
                                              .priority = 0};
    pending.deliverGeneratedBlockTicks(std::array{tick}, chunks.accessor());
    REQUIRE(east.scheduledBlockTicks().size() == 1);
    CHECK(east.scheduledBlockTicks()[0].localX == 0);
    CHECK(east.scheduledBlockTicks()[0].delay == 5);

    // And one whose block is gone is not scheduled.
    world::Chunk& west = chunks.add(-1, 0);
    world::ChunkGeneratedBlockTick orphan = tick;
    orphan.x = -1;
    pending.deliverGeneratedBlockTicks(std::array{orphan}, chunks.accessor());
    CHECK(west.scheduledBlockTicks().empty());
}

TEST_CASE("the mobs a template brought wait for the tick loop to name them") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    CHECK(pending.takePendingGeneratedEntities().empty());

    world::ChunkGeneratedEntity mob;
    mob.type = Identifier::vanilla("zombie");
    pending.addPendingGeneratedEntities({mob});
    const std::vector<world::ChunkGeneratedEntity> taken = pending.takePendingGeneratedEntities();
    REQUIRE(taken.size() == 1);
    CHECK(taken[0].type.path() == "zombie");
    // Taken once, so the chunk's own copy cannot be spawned twice.
    CHECK(pending.takePendingGeneratedEntities().empty());
}

TEST_CASE("a generated entity keeps the identity it was first given") {
    std::mutex mutex;
    PendingChunkDecorations pending{mutex};
    LoadedChunks chunks;
    world::Chunk& here = chunks.add(0, 0);
    world::ChunkGeneratedEntity mob;
    mob.type = Identifier::vanilla("zombie");
    here.addGeneratedEntity(mob);
    chunks.add(4, 4).addGeneratedEntity(mob);

    std::int32_t nextId = 100;
    const auto allocate = [&nextId] { return nextId++; };

    const std::vector<GeneratedWorldEntity> first = pending.generatedEntities(allocate, 0, 0, 1, chunks.accessor());
    REQUIRE(first.size() == 1);
    CHECK(first[0].entityId == 100);
    CHECK_FALSE(first[0].uuid.isNil());

    // Asked again, the same mob is the same mob: a client that saw two would draw two.
    const std::vector<GeneratedWorldEntity> again = pending.generatedEntities(allocate, 0, 0, 1, chunks.accessor());
    REQUIRE(again.size() == 1);
    CHECK(again[0].entityId == 100);
    CHECK(again[0].uuid == first[0].uuid);

    // A chunk outside the radius is not walked at all.
    CHECK(nextId == 101);
    const std::vector<GeneratedWorldEntity> wider = pending.generatedEntities(allocate, 2, 2, 3, chunks.accessor());
    CHECK(wider.size() == 2);

    // And when the chunk goes, so does what it named.
    pending.forgetGeneratedEntities(PendingChunkDecorations::key(0, 0));
    const std::vector<GeneratedWorldEntity> renamed = pending.generatedEntities(allocate, 0, 0, 0, chunks.accessor());
    REQUIRE(renamed.size() == 1);
    CHECK(renamed[0].entityId != 100);
    // The other chunk's own entity kept its identity through that.
    CHECK(pending.generatedEntities(allocate, 4, 4, 0, chunks.accessor())[0].entityId == 101);
}

TEST_CASE("the key a chunk is filed under is the one the map uses") {
    CHECK(PendingChunkDecorations::key(0, 0) == 0);
    CHECK(PendingChunkDecorations::key(1, 0) != PendingChunkDecorations::key(0, 1));
    // Negative coordinates survive it, which is most of the world.
    CHECK(PendingChunkDecorations::key(-1, -1) != PendingChunkDecorations::key(-1, 1));
    CHECK(PendingChunkDecorations::key(-3, 7) == PendingChunkDecorations::key(-3, 7));
}
