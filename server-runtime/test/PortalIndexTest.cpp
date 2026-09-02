#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PortalIndex.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <doctest.h>

#include <filesystem>
#include <fstream>
#include <string>

using basalt::server::runtime::world::PortalIndex;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t portalState() {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("nether_portal"), {{"axis", "x"}}, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string scratch(std::string_view name) {
    const std::filesystem::path directory = std::filesystem::path{BASALT_TEST_SCRATCH_DIR} / name;
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::filesystem::create_directories(directory, ignored);
    return directory.string();
}

/// A chunk with a portal block at each world position asked for.
[[nodiscard]] Chunk chunkWith(std::int32_t chunkX, std::int32_t chunkZ,
                              const std::vector<std::array<std::int32_t, 3>>& blocks) {
    Chunk chunk{chunkX, chunkZ};
    for (const std::array<std::int32_t, 3>& at : blocks) {
        REQUIRE(chunk.setBlockStateId(at[0] & 15, at[1], at[2] & 15, portalState()));
    }
    return chunk;
}

} // namespace

TEST_CASE("a scan finds every portal block in the chunk and nothing else") {
    PortalIndex index;
    CHECK_FALSE(index.dirty());
    CHECK(index.size() == 0);

    index.scan(chunkWith(0, 0, {{3, 64, 5}, {3, 65, 5}, {4, 70, 9}}));

    CHECK(index.size() == 3);
    CHECK(index.dirty());
    CHECK(index.nearest(3, 64, 5, 1) == std::array<std::int32_t, 3>{3, 64, 5});
    CHECK(index.nearest(4, 70, 9, 0) == std::array<std::int32_t, 3>{4, 70, 9});
}

TEST_CASE("a scan reaches the bottom section and the first block of every row") {
    PortalIndex index;

    // The lowest section and the corner of it, which is where an off-by-one loop starts too late.
    index.scan(chunkWith(0, 0, {{0, -64, 0}, {15, -49, 15}}));

    CHECK(index.size() == 2);
    CHECK(index.nearest(0, -64, 0, 1) == std::array<std::int32_t, 3>{0, -64, 0});
    CHECK(index.nearest(15, -49, 15, 1) == std::array<std::int32_t, 3>{15, -49, 15});
}

TEST_CASE("two chunks that differ only in the sign of one coordinate stay apart") {
    PortalIndex index;
    index.scan(chunkWith(0, -1, {{4, 64, -12}}));
    index.scan(chunkWith(1, -1, {{20, 64, -12}}));

    CHECK(index.size() == 2);
    CHECK(index.nearest(4, 64, -12, 1) == std::array<std::int32_t, 3>{4, 64, -12});
    CHECK(index.nearest(20, 64, -12, 1) == std::array<std::int32_t, 3>{20, 64, -12});
}

TEST_CASE("two portals the same distance away answer with the first one found") {
    PortalIndex index;
    index.scan(chunkWith(0, 0, {{7, 64, 8}, {9, 64, 8}}));

    // parity: a later one has to be strictly nearer to win, which makes the answer stable.
    CHECK(index.nearest(8, 64, 8, 4) == std::array<std::int32_t, 3>{7, 64, 8});
}

TEST_CASE("a scan of a chunk with nothing in it says so") {
    PortalIndex index;

    index.scan(Chunk{0, 0});

    CHECK(index.size() == 0);
    CHECK_FALSE(index.nearest(0, 64, 0, 32).has_value());
}

TEST_CASE("a second scan replaces what the first one said") {
    PortalIndex index;
    index.scan(chunkWith(0, 0, {{3, 64, 5}, {3, 65, 5}}));
    REQUIRE(index.size() == 2);

    index.scan(chunkWith(0, 0, {{8, 40, 8}}));
    CHECK(index.size() == 1);
    CHECK(index.nearest(8, 40, 8, 0) == std::array<std::int32_t, 3>{8, 40, 8});
    CHECK_FALSE(index.nearest(3, 64, 5, 0).has_value());

    // And a chunk that has lost its portals loses its entry outright.
    index.scan(Chunk{0, 0});
    CHECK(index.size() == 0);
}

TEST_CASE("a scan that changes nothing leaves the index clean") {
    PortalIndex index;
    index.scan(chunkWith(0, 0, {{3, 64, 5}}));
    index.save(scratch("portal-index-unchanged"));
    REQUIRE_FALSE(index.dirty());

    index.scan(chunkWith(0, 0, {{3, 64, 5}}));

    CHECK_FALSE(index.dirty());
}

TEST_CASE("the nearest portal is the nearest one, not the first found") {
    PortalIndex index;
    index.scan(chunkWith(0, 0, {{1, 64, 1}, {14, 64, 14}}));
    index.scan(chunkWith(1, 0, {{17, 64, 1}}));

    // Distance is counted in three dimensions, so height counts as much as anything else.
    CHECK(index.nearest(15, 64, 1, 8) == std::array<std::int32_t, 3>{17, 64, 1});
    CHECK(index.nearest(0, 64, 0, 8) == std::array<std::int32_t, 3>{1, 64, 1});
    CHECK(index.nearest(15, 20, 14, 8) == std::array<std::int32_t, 3>{14, 64, 14});
}

TEST_CASE("the radius is a square around the point, and nothing outside it counts") {
    PortalIndex index;
    index.scan(chunkWith(0, 0, {{10, 64, 4}}));

    CHECK(index.nearest(4, 64, 4, 6) == std::array<std::int32_t, 3>{10, 64, 4});
    CHECK_FALSE(index.nearest(4, 64, 4, 5).has_value());
    CHECK(index.nearest(10, 64, 10, 6) == std::array<std::int32_t, 3>{10, 64, 4});
    CHECK_FALSE(index.nearest(10, 64, 10, 5).has_value());
    // Height is not part of the radius at all: only the distance uses it.
    CHECK(index.nearest(10, 200, 4, 0) == std::array<std::int32_t, 3>{10, 64, 4});
}

TEST_CASE("what was scanned comes back after a save and a load") {
    const std::string root = scratch("portal-index-round-trip");
    PortalIndex saved;
    saved.scan(chunkWith(0, 0, {{3, 64, 5}, {3, 65, 5}}));
    saved.scan(chunkWith(-1, 2, {{-12, -40, 34}}));
    saved.save(root);
    CHECK_FALSE(saved.dirty());

    PortalIndex loaded;
    loaded.load(root);

    CHECK(loaded.size() == 3);
    CHECK_FALSE(loaded.dirty());
    CHECK(loaded.nearest(3, 64, 5, 1) == std::array<std::int32_t, 3>{3, 64, 5});
    // Negative coordinates survive the packing, which is what the sign bits are there for.
    CHECK(loaded.nearest(-12, -40, 34, 1) == std::array<std::int32_t, 3>{-12, -40, 34});
}

TEST_CASE("a save with nothing to say does not write, and a missing file is not a failure") {
    const std::string root = scratch("portal-index-empty");
    PortalIndex index;

    index.save(root);
    CHECK_FALSE(std::filesystem::exists(std::filesystem::path{root} / "portals.nbt"));

    index.load(root);
    CHECK(index.size() == 0);
    CHECK_FALSE(index.dirty());
}

TEST_CASE("an entry whose name is not a chunk key is skipped") {
    const std::string root = scratch("portal-index-bad-key");
    basalt::wire::nbt::NbtCompound written;
    written.putLongArray("12abc", {1});
    written.putLongArray("", {2});
    written.putLongArray("0", {3});
    REQUIRE(basalt::wire::io::fileIO::writeBytes(
        (std::filesystem::path{root} / "portals.nbt").string(),
        basalt::wire::nbt::nbtIO::write(basalt::wire::nbt::NbtTag::ofCompound(std::move(written)))));
    PortalIndex index;

    index.load(root);

    // Only the one that reads as a whole number all the way through is kept.
    CHECK(index.size() == 1);
}

TEST_CASE("a file that is not the index at all is ignored rather than believed") {
    const std::string root = scratch("portal-index-broken");
    const std::filesystem::path file = std::filesystem::path{root} / "portals.nbt";
    std::ofstream{file} << "not nbt";
    PortalIndex index;

    index.load(root);

    CHECK(index.size() == 0);
}
