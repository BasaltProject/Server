#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Jukeboxes.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] basalt::wire::common::Identifier item(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

[[nodiscard]] const basalt::wire::nbt::NbtCompound& dataAt(WorldState& world, std::int32_t x, std::int32_t y,
                                                           std::int32_t z) {
    const basalt::server::world::Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    REQUIRE(chunk != nullptr);
    const basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
    REQUIRE(entity != nullptr);
    return entity->data();
}

} // namespace

TEST_CASE("a disc is named after the song it plays") {
    CHECK(jukeboxes::songOf(item("music_disc_13")) == "13");
    CHECK(jukeboxes::songOf(item("music_disc_otherside")) == "otherside");
    CHECK(jukeboxes::isDisc(item("music_disc_11")));
    CHECK_FALSE(jukeboxes::isDisc(item("stone")));
    CHECK_FALSE(jukeboxes::isDisc(item("jukebox")));
}

TEST_CASE("a disc goes in, plays, and comes back out") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t quiet = stateOf("jukebox", {{"has_record", "false"}});
    const std::int32_t playing = stateOf("jukebox", {{"has_record", "true"}});
    REQUIRE(world.setBlockState(8, 64, 8, quiet));

    const std::optional<BlockChange> started = jukeboxes::insert(world, 8, 64, 8, item("music_disc_13"), 13);
    REQUIRE(started.has_value());
    CHECK(started->stateId == playing);
    CHECK(world.blockState(8, 64, 8) == playing);
    CHECK(jukeboxes::songAt(world, 8, 64, 8) == "13");
    CHECK(jukeboxes::comparatorOutputAt(dataAt(world, 8, 64, 8)) == 13);

    const std::optional<std::pair<BlockChange, basalt::wire::common::Identifier>> stopped =
        jukeboxes::eject(world, 8, 64, 8);
    REQUIRE(stopped.has_value());
    CHECK(stopped->first.stateId == quiet);
    CHECK(stopped->second == item("music_disc_13"));
    CHECK(world.blockState(8, 64, 8) == quiet);
    CHECK_FALSE(jukeboxes::songAt(world, 8, 64, 8).has_value());
    CHECK(jukeboxes::comparatorOutputAt(dataAt(world, 8, 64, 8)) == 0);
}

TEST_CASE("a jukebox that is already playing takes no second disc") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("jukebox", {{"has_record", "false"}})));
    REQUIRE(jukeboxes::insert(world, 8, 64, 8, item("music_disc_cat"), 1).has_value());

    CHECK_FALSE(jukeboxes::insert(world, 8, 64, 8, item("music_disc_13"), 13).has_value());
    CHECK(jukeboxes::songAt(world, 8, 64, 8) == "cat");
}

TEST_CASE("an empty jukebox has nothing to eject, and neither has anything else") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("jukebox", {{"has_record", "false"}})));
    CHECK_FALSE(jukeboxes::eject(world, 8, 64, 8).has_value());

    REQUIRE(world.setBlockState(4, 64, 4, registry::blocks::named("stone")->stateId));
    CHECK_FALSE(jukeboxes::insert(world, 4, 64, 4, item("music_disc_13"), 13).has_value());
    CHECK_FALSE(jukeboxes::eject(world, 4, 64, 4).has_value());
    CHECK_FALSE(jukeboxes::songAt(world, 4, 64, 4).has_value());

    // And a position with no chunk under it answers no rather than reaching for one.
    CHECK_FALSE(jukeboxes::insert(world, 300, 64, 300, item("music_disc_13"), 13).has_value());
}

TEST_CASE("a jukebox away from the middle chunk is found where it stands") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 1);
    const std::int32_t quiet = stateOf("jukebox", {{"has_record", "false"}});
    // Twenty-one is in the next chunk over and its low bits are odd, so both halves of the split
    // have to be right for the block entity to land on the block.
    REQUIRE(world.setBlockState(21, 64, 21, quiet));

    REQUIRE(jukeboxes::insert(world, 21, 64, 21, item("music_disc_cat"), 1).has_value());

    CHECK(jukeboxes::songAt(world, 21, 64, 21) == "cat");
    CHECK(jukeboxes::comparatorOutputAt(dataAt(world, 21, 64, 21)) == 1);
    CHECK(jukeboxes::eject(world, 21, 64, 21).has_value());
}

TEST_CASE("asking an empty jukebox what it plays leaves it as it was") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("jukebox", {{"has_record", "false"}})));

    CHECK_FALSE(jukeboxes::songAt(world, 8, 64, 8).has_value());
    CHECK_FALSE(jukeboxes::eject(world, 8, 64, 8).has_value());

    // Neither question makes a block entity that was not there.
    const basalt::server::world::Chunk* chunk = world.chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    CHECK(chunk->blockEntityAt(8, 64, 8) == nullptr);
}

TEST_CASE("the song's number comes out of the jar once and is kept") {
    std::int32_t reads = 0;
    jukeboxes::SongOutputs outputs{[&](std::string_view name) -> std::optional<std::string> {
        ++reads;
        if (name == "data/minecraft/jukebox_song/13.json") {
            return std::string{R"({"comparator_output": 13, "length_in_seconds": 178})"};
        }
        if (name == "data/minecraft/jukebox_song/broken.json") {
            return std::string{R"({"length_in_seconds": 1})"};
        }
        // Written without the space after the colon, and with nothing after the number.
        if (name == "data/minecraft/jukebox_song/tight.json") {
            return std::string{R"({"length_in_seconds":1,"comparator_output":11)"};
        }
        return std::nullopt;
    }};

    CHECK(outputs.of("13") == 13);
    CHECK(outputs.of("13") == 13);
    CHECK(reads == 1);
    // A song whose file says nothing about it, and one with no file at all, both read as nothing.
    CHECK(outputs.of("broken") == 0);
    CHECK(outputs.of("missing") == 0);
    CHECK(reads == 3);
    CHECK(outputs.of("tight") == 11);
}

TEST_CASE("with no jar to read, every song is nothing") {
    jukeboxes::SongOutputs outputs{{}};

    CHECK(outputs.of("13") == 0);
    CHECK(outputs.of("cat") == 0);
}
