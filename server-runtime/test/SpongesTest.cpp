#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Sponges.hpp>

#include <doctest.h>

#include <map>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A world with the middle chunk in memory, since `WorldState` cannot be moved out of a helper.
struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

/// Fills a box with water, which is what a sponge is dropped into.
void sea(WorldState& world, std::int32_t radius) {
    for (std::int32_t x = 8 - radius; x <= 8 + radius; ++x) {
        for (std::int32_t y = 64 - radius; y <= 64 + radius; ++y) {
            for (std::int32_t z = 8 - radius; z <= 8 + radius; ++z) {
                REQUIRE(world.setBlockState(x, y, z, stateOf("water")));
            }
        }
    }
}

[[nodiscard]] std::int32_t waterAround(WorldState& world, std::int32_t radius) {
    std::int32_t found = 0;
    for (std::int32_t x = 8 - radius; x <= 8 + radius; ++x) {
        for (std::int32_t y = 64 - radius; y <= 64 + radius; ++y) {
            for (std::int32_t z = 8 - radius; z <= 8 + radius; ++z) {
                if (world.blockState(x, y, z) == stateOf("water")) {
                    ++found;
                }
            }
        }
    }
    return found;
}

} // namespace

TEST_CASE("a sponge drinks sixty-five blocks of water and turns wet") {
    Loaded loaded;
    WorldState& world = loaded.state;
    sea(world, 6);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sponge")));
    std::vector<std::int32_t> changed;

    sponges::absorb(world, 8, 64, 8, [&](std::int32_t stateId, std::int32_t, std::int32_t, std::int32_t) {
        changed.push_back(stateId);
    });

    // Sixty-five blocks of water, plus the sponge itself becoming wet.
    CHECK(changed.size() == 66);
    CHECK(changed.back() == stateOf("wet_sponge"));
    CHECK(world.blockState(8, 64, 8) == stateOf("wet_sponge"));
}

TEST_CASE("a dry sponge in the dry stays dry") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sponge")));
    std::int32_t changes = 0;

    sponges::absorb(world, 8, 64, 8, [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t) { ++changes; });

    CHECK(changes == 0);
    CHECK(world.blockState(8, 64, 8) == stateOf("sponge"));
}

TEST_CASE("only a sponge drinks") {
    Loaded loaded;
    WorldState& world = loaded.state;
    sea(world, 2);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("wet_sponge")));
    std::int32_t changes = 0;

    sponges::absorb(world, 8, 64, 8, [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t) { ++changes; });

    CHECK(changes == 0);
    CHECK(waterAround(world, 2) == 124);
}

TEST_CASE("a sponge reaches every one of the six sides") {
    const auto drinksFrom = [](std::int32_t dx, std::int32_t dy, std::int32_t dz) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8 + dx, 64 + dy, 8 + dz, stateOf("water")));
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("sponge")));
        std::int32_t changes = 0;
        sponges::absorb(world, 8, 64, 8, [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t) { ++changes; });
        return changes == 2 && world.blockState(8 + dx, 64 + dy, 8 + dz) == air();
    };

    CHECK(drinksFrom(0, -1, 0));
    CHECK(drinksFrom(0, 1, 0));
    CHECK(drinksFrom(0, 0, -1));
    CHECK(drinksFrom(0, 0, 1));
    CHECK(drinksFrom(-1, 0, 0));
    CHECK(drinksFrom(1, 0, 0));

    // Diagonals are not sides, so nothing there is reached in one step.
    CHECK_FALSE(drinksFrom(1, 1, 0));
    CHECK_FALSE(drinksFrom(1, 0, 1));
}

TEST_CASE("the search stops six blocks out") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // A single line of water running further than the sponge can reach.
    for (std::int32_t step = 1; step <= 8; ++step) {
        REQUIRE(world.setBlockState(8 - step, 64, 8, stateOf("water")));
    }
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sponge")));
    std::int32_t changes = 0;

    sponges::absorb(world, 8, 64, 8, [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t) { ++changes; });

    // Six of water, and the seventh is out of reach.
    CHECK(changes == 7);
    CHECK(world.blockState(1, 64, 8) == stateOf("water"));
    CHECK(world.blockState(2, 64, 8) == air());
}

TEST_CASE("kelp and seagrass come up with the water") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("kelp_plant")));
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("seagrass")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("tall_seagrass", {{"half", "lower"}})));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sponge")));
    std::int32_t changes = 0;

    sponges::absorb(world, 8, 64, 8, [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t) { ++changes; });

    CHECK(changes == 4);
    CHECK(world.blockState(9, 64, 8) == air());
    CHECK(world.blockState(7, 64, 8) == air());
    CHECK(world.blockState(8, 65, 8) == air());
}

TEST_CASE("a logged block is drained rather than broken") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t logged = stateOf("oak_slab", {{"type", "bottom"}, {"waterlogged", "true"}});
    const std::int32_t dry = stateOf("oak_slab", {{"type", "bottom"}, {"waterlogged", "false"}});
    REQUIRE(world.setBlockState(9, 64, 8, logged));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("sponge")));
    std::int32_t changes = 0;

    sponges::absorb(world, 8, 64, 8, [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t) { ++changes; });

    CHECK(changes == 2);
    CHECK(world.blockState(9, 64, 8) == dry);
}

TEST_CASE("a wet sponge dries out where water evaporates") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("wet_sponge")));
    std::int32_t changes = 0;
    const sponges::OnChanged count = [&](std::int32_t, std::int32_t, std::int32_t, std::int32_t) { ++changes; };

    sponges::dryIfEvaporating(world, 8, 64, 8, count, false);
    CHECK(changes == 0);
    CHECK(world.blockState(8, 64, 8) == stateOf("wet_sponge"));

    sponges::dryIfEvaporating(world, 8, 64, 8, count, true);
    CHECK(changes == 1);
    CHECK(world.blockState(8, 64, 8) == stateOf("sponge"));

    // A dry one has nothing left to dry.
    sponges::dryIfEvaporating(world, 8, 64, 8, count, true);
    CHECK(changes == 1);
}
