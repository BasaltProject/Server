#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/PortalIndex.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/wire/io/FileIO.hpp>
#include <basalt/wire/nbt/NbtIO.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <doctest.h>

#include <cstdint>
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

[[nodiscard]] Chunk chunkWith(std::int32_t chunkX, std::int32_t chunkZ,
                              const std::vector<std::array<std::int32_t, 3>>& blocks) {
    Chunk chunk{chunkX, chunkZ};
    for (const std::array<std::int32_t, 3>& at : blocks) {
        REQUIRE(chunk.setBlockStateId(at[0] & 15, at[1], at[2] & 15, portalState()));
    }
    return chunk;
}

} // namespace

TEST_CASE("the nearest portal is the nearest one, not the first found") {
    PortalIndex index;
    index.scan(chunkWith(0, 0, {{1, 64, 1}, {14, 64, 14}}));
    index.scan(chunkWith(1, 0, {{17, 64, 1}}));

    CHECK(index.nearest(15, 64, 1, 8) == std::array<std::int32_t, 3>{17, 64, 1});
    CHECK(index.nearest(0, 64, 0, 8) == std::array<std::int32_t, 3>{1, 64, 1});
    CHECK(index.nearest(15, 20, 14, 8) == std::array<std::int32_t, 3>{14, 64, 14});
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
    CHECK(loaded.nearest(-12, -40, 34, 1) == std::array<std::int32_t, 3>{-12, -40, 34});
}

TEST_CASE("a file that is not the index at all is ignored rather than believed") {
    const std::string root = scratch("portal-index-broken");
    const std::filesystem::path file = std::filesystem::path{root} / "portals.nbt";
    std::ofstream{file} << "not nbt";
    PortalIndex index;

    index.load(root);

    CHECK(index.size() == 0);
}
