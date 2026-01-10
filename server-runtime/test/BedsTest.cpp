#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Beds.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::runtime::config::WorldDimension;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t bed(const std::string& facing, const std::string& part,
                               const std::string& occupied = "false") {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("red_bed"), {{"facing", facing}, {"part", part}, {"occupied", occupied}},
        stateOf("red_bed"));
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }

    void put(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        REQUIRE(state.setBlockState(x, y, z, stateId));
    }
};

} // namespace

TEST_CASE("every colour of bed is a bed, and so is the item") {
    CHECK(beds::isBed(stateOf("red_bed")));
    CHECK(beds::isBed(stateOf("white_bed")));
    CHECK(beds::isBed(stateOf("light_blue_bed")));
    CHECK_FALSE(beds::isBed(stateOf("stone")));
    CHECK_FALSE(beds::isBed(registry::blocks::air().stateId));

    CHECK(beds::isBedItem(Identifier::vanilla("red_bed")));
    CHECK(beds::isBedItem(Identifier::vanilla("light_gray_bed")));
    CHECK_FALSE(beds::isBedItem(Identifier::vanilla("stone")));
    CHECK_FALSE(beds::isBedItem(Identifier::vanilla("bedrock")));
}

TEST_CASE("a bed says which way it lies, which half it is and whether anyone is in it") {
    CHECK(beds::facingOf(bed("north", "head")) == "north");
    CHECK(beds::facingOf(bed("east", "foot")) == "east");
    CHECK(beds::facingOf(stateOf("stone")).empty());

    CHECK(beds::isHead(bed("north", "head")));
    CHECK_FALSE(beds::isHead(bed("north", "foot")));
    CHECK_FALSE(beds::isHead(stateOf("stone")));

    CHECK(beds::isOccupied(bed("north", "head", "true")));
    CHECK_FALSE(beds::isOccupied(bed("north", "head", "false")));
    CHECK_FALSE(beds::isOccupied(stateOf("stone")));
}

TEST_CASE("setting the occupied flag keeps everything else about the bed") {
    const std::int32_t empty = bed("east", "head", "false");

    const std::int32_t taken = beds::withOccupied(empty, true);

    CHECK(beds::isOccupied(taken));
    CHECK(beds::facingOf(taken) == "east");
    CHECK(beds::isHead(taken));
    CHECK(beds::withOccupied(taken, false) == empty);
    // A block with no such property is handed back as it came.
    CHECK(beds::withOccupied(stateOf("stone"), true) == stateOf("stone"));
}

TEST_CASE("the pillow is one block along the way the bed faces") {
    CHECK(beds::headOffset(bed("north", "foot")) == std::array<std::int32_t, 3>{0, 0, -1});
    CHECK(beds::headOffset(bed("south", "foot")) == std::array<std::int32_t, 3>{0, 0, 1});
    CHECK(beds::headOffset(bed("west", "foot")) == std::array<std::int32_t, 3>{-1, 0, 0});
    CHECK(beds::headOffset(bed("east", "foot")) == std::array<std::int32_t, 3>{1, 0, 0});
    // Something that is not a bed points nowhere.
    CHECK(beds::headOffset(stateOf("stone")) == std::array<std::int32_t, 3>{0, 0, 0});
}

TEST_CASE("a bed works in the overworld and blows up anywhere else") {
    CHECK_FALSE(beds::explodesIn(WorldDimension::Overworld));
    CHECK(beds::explodesIn(WorldDimension::Nether));
    CHECK(beds::explodesIn(WorldDimension::End));
}

TEST_CASE("night is the window a bed takes anyone in, and it comes round every day") {
    // parity: `Level.isNight`, whose window is 12542 to 23459 inclusive.
    CHECK_FALSE(beds::isNight(12541));
    CHECK(beds::isNight(12542));
    CHECK(beds::isNight(18000));
    CHECK(beds::isNight(23459));
    CHECK_FALSE(beds::isNight(23460));
    CHECK_FALSE(beds::isNight(0));

    // The same window on the next day, and the one after that.
    CHECK(beds::isNight(24000 + 13000));
    CHECK_FALSE(beds::isNight(24000 + 1000));
    CHECK(beds::isNight((24000 * 7) + 12542));
}

TEST_CASE("waking up puts the clock at the next morning, whatever hour it was") {
    CHECK(beds::nextMorning(0) == 24000);
    CHECK(beds::nextMorning(13000) == 24000);
    CHECK(beds::nextMorning(23999) == 24000);
    CHECK(beds::nextMorning(24000) == 48000);
    CHECK(beds::nextMorning(24001) == 48000);
}

TEST_CASE("destroying a bed takes both of its halves away") {
    Loaded loaded;
    loaded.put(8, 64, 8, bed("south", "foot"));
    loaded.put(8, 64, 9, bed("south", "head"));

    const std::vector<BlockChange> removed = beds::destroy(loaded.state, 8, 64, 8);

    REQUIRE(removed.size() == 2);
    CHECK(removed[0] == BlockChange{.x = 8, .y = 64, .z = 8, .stateId = registry::blocks::air().stateId});
    CHECK(removed[1] == BlockChange{.x = 8, .y = 64, .z = 9, .stateId = registry::blocks::air().stateId});
    CHECK(loaded.state.blockState(8, 64, 8) == registry::blocks::air().stateId);
    CHECK(loaded.state.blockState(8, 64, 9) == registry::blocks::air().stateId);
}

TEST_CASE("destroying it from the pillow end works the same way round") {
    Loaded loaded;
    loaded.put(8, 64, 8, bed("south", "foot"));
    loaded.put(8, 64, 9, bed("south", "head"));

    const std::vector<BlockChange> removed = beds::destroy(loaded.state, 8, 64, 9);

    REQUIRE(removed.size() == 2);
    CHECK(removed[0].z == 9);
    CHECK(removed[1].z == 8);
    CHECK(loaded.state.blockState(8, 64, 8) == registry::blocks::air().stateId);
}

TEST_CASE("a bed lying the other way is taken away along its own axis") {
    Loaded loaded;
    loaded.put(8, 64, 8, bed("east", "foot"));
    loaded.put(9, 64, 8, bed("east", "head"));

    const std::vector<BlockChange> removed = beds::destroy(loaded.state, 8, 64, 8);

    REQUIRE(removed.size() == 2);
    CHECK(removed[0] == BlockChange{.x = 8, .y = 64, .z = 8, .stateId = registry::blocks::air().stateId});
    CHECK(removed[1] == BlockChange{.x = 9, .y = 64, .z = 8, .stateId = registry::blocks::air().stateId});

    // And from the pillow end of the same bed, which steps the other way.
    Loaded fromHead;
    fromHead.put(8, 64, 8, bed("east", "foot"));
    fromHead.put(9, 64, 8, bed("east", "head"));
    const std::vector<BlockChange> backwards = beds::destroy(fromHead.state, 9, 64, 8);
    REQUIRE(backwards.size() == 2);
    CHECK(backwards[0] == BlockChange{.x = 9, .y = 64, .z = 8, .stateId = registry::blocks::air().stateId});
    CHECK(backwards[1] == BlockChange{.x = 8, .y = 64, .z = 8, .stateId = registry::blocks::air().stateId});
}

TEST_CASE("half a bed with nothing on the other end takes only itself away") {
    Loaded loaded;
    loaded.put(8, 64, 8, bed("south", "foot"));

    const std::vector<BlockChange> removed = beds::destroy(loaded.state, 8, 64, 8);

    REQUIRE(removed.size() == 1);
    CHECK(removed[0] == BlockChange{.x = 8, .y = 64, .z = 8, .stateId = registry::blocks::air().stateId});
}

TEST_CASE("what is not a bed is not destroyed") {
    Loaded loaded;
    loaded.put(8, 64, 8, stateOf("stone"));

    CHECK(beds::destroy(loaded.state, 8, 64, 8).empty());
    CHECK(loaded.state.blockState(8, 64, 8) == stateOf("stone"));
    // And neither is a block outside the loaded world.
    CHECK(beds::destroy(loaded.state, 500, 64, 500).empty());
}

TEST_CASE("each way a bed can refuse has its own message") {
    CHECK(translationKeyOf(BedProblem::NotPossibleNow) == "block.minecraft.bed.no_sleep");
    CHECK(translationKeyOf(BedProblem::TooFarAway) == "block.minecraft.bed.too_far_away");
    CHECK(translationKeyOf(BedProblem::Occupied) == "block.minecraft.bed.occupied");
    CHECK(translationKeyOf(BedProblem::NotHere) == "block.minecraft.bed.not_valid");
}
