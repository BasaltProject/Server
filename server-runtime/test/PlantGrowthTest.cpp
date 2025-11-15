#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/PlantGrowth.hpp>

#include <doctest.h>

#include <array>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t agedState(const std::string& path, std::int32_t age) {
    const std::optional<std::int32_t> state = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla(path), {{"age", std::to_string(age)}}, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

} // namespace

TEST_CASE("kelp climbs through water and leaves a stem behind it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t y = 60; y < 70; ++y) {
        REQUIRE(world.setBlockState(4, y, 4, stateOf("water")));
    }
    REQUIRE(world.setBlockState(4, 62, 4, agedState("kelp", 0)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    // A chance of one, so the only thing deciding is whether there is room.
    climbingPlantGrowth::growingPlant(world, 4, 62, 4, *world.blockState(4, 62, 4), true, 1.0, random, changes);

    CHECK(pathOf(*world.blockState(4, 63, 4)) == "kelp");
    CHECK(growthSupport::ageOf(*world.blockState(4, 63, 4)) == 1);
    // parity: what it leaves behind is the plant block, not the head.
    CHECK(pathOf(*world.blockState(4, 62, 4)) == "kelp_plant");
    CHECK(changes.size() == 2);
}

TEST_CASE("a climbing plant stops at a block, at the top of its age and against the odds") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t y = 60; y < 70; ++y) {
        REQUIRE(world.setBlockState(4, y, 4, stateOf("water")));
    }

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;

    // Something in the way.
    REQUIRE(world.setBlockState(4, 62, 4, agedState("kelp", 0)));
    REQUIRE(world.setBlockState(4, 63, 4, stateOf("stone")));
    climbingPlantGrowth::growingPlant(world, 4, 62, 4, *world.blockState(4, 62, 4), true, 1.0, random, changes);
    CHECK(changes.empty());
    REQUIRE(world.setBlockState(4, 63, 4, stateOf("water")));

    // Already as old as it gets.
    REQUIRE(world.setBlockState(4, 62, 4, agedState("kelp", climbingPlantGrowth::kMaxAge)));
    climbingPlantGrowth::growingPlant(world, 4, 62, 4, *world.blockState(4, 62, 4), true, 1.0, random, changes);
    CHECK(changes.empty());

    // A chance of nothing never grows.
    REQUIRE(world.setBlockState(4, 62, 4, agedState("kelp", 0)));
    for (std::int32_t at = 0; at < 100; ++at) {
        climbingPlantGrowth::growingPlant(world, 4, 62, 4, *world.blockState(4, 62, 4), true, 0.0, random, changes);
    }
    CHECK(changes.empty());
}

TEST_CASE("weeping vines climb downwards through air, and kelp will not") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(6, 70, 6, agedState("weeping_vines", 0)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    std::vector<BlockChange> changes;
    climbingPlantGrowth::growingPlant(world, 6, 70, 6, *world.blockState(6, 70, 6), false, 1.0, random, changes);
    CHECK(pathOf(*world.blockState(6, 69, 6)) == "weeping_vines");
    CHECK(pathOf(*world.blockState(6, 70, 6)) == "weeping_vines_plant");

    // Kelp needs water, so the same air stops it dead.
    REQUIRE(world.setBlockState(8, 70, 8, agedState("kelp", 0)));
    changes.clear();
    climbingPlantGrowth::growingPlant(world, 8, 70, 8, *world.blockState(8, 70, 8), true, 1.0, random, changes);
    CHECK(changes.empty());
}

TEST_CASE("copper weathers a stage at a time") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(2, 64, 2, stateOf("copper_block")));

    rng::RandomSource random = rng::RandomSource::legacy(12345);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 20000 && changes.empty(); ++at) {
        weatheringGrowth::weather(world, 2, 64, 2, *world.blockState(2, 64, 2), random, changes);
    }
    REQUIRE(changes.size() == 1);
    CHECK(pathOf(*world.blockState(2, 64, 2)) == "exposed_copper");
}

TEST_CASE("a younger neighbour keeps copper as it is") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: one block younger than this one within four stops it weathering at all, which is
    // what makes a wall of fresh copper age evenly instead of in patches.
    REQUIRE(world.setBlockState(2, 64, 2, stateOf("exposed_copper")));
    REQUIRE(world.setBlockState(4, 64, 2, stateOf("copper_block")));

    rng::RandomSource random = rng::RandomSource::legacy(12345);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 20000; ++at) {
        weatheringGrowth::weather(world, 2, 64, 2, *world.blockState(2, 64, 2), random, changes);
    }
    CHECK(changes.empty());
    CHECK(pathOf(*world.blockState(2, 64, 2)) == "exposed_copper");

    // Two east and three up is five in total, which is out of reach: the shape is a diamond, so
    // four on one axis is as far as it gets and two on two axes is not.
    REQUIRE(world.setBlockState(4, 64, 2, registry::blocks::air().stateId));
    REQUIRE(world.setBlockState(4, 67, 2, stateOf("copper_block")));
    for (std::int32_t at = 0; at < 20000 && changes.empty(); ++at) {
        weatheringGrowth::weather(world, 2, 64, 2, *world.blockState(2, 64, 2), random, changes);
    }
    CHECK(changes.size() == 1);
}

TEST_CASE("how soon copper weathers is what its neighbours make it") {
    // The same sequence for each, so what changes the answer is the neighbours and nothing else.
    const auto callsToWeather = [](const std::vector<std::array<std::int32_t, 4>>& neighbours) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("exposed_copper")));
        for (const std::array<std::int32_t, 4>& at : neighbours) {
            REQUIRE(world.setBlockState(at[0], at[1], at[2], at[3]));
        }
        rng::RandomSource random = rng::RandomSource::legacy(12345);
        std::vector<BlockChange> changes;
        std::int32_t calls = 0;
        while (changes.empty() && calls < 100000) {
            weatheringGrowth::weather(world, 8, 64, 8, *world.blockState(8, 64, 8), random, changes);
            ++calls;
        }
        return calls;
    };

    // parity: about once a day for a block with nothing around it, which is a roll of one in
    // seventeen and no share to beat.
    const std::int32_t alone = callsToWeather({});
    CHECK(alone == 6);

    // An older neighbour does not hold it back: the share counts what is further along, so a block
    // among oxidized copper weathers exactly as readily as one on its own.
    CHECK(callsToWeather({{9, 64, 8, stateOf("oxidized_copper")}}) == alone);

    // Blocks of its own stage do hold it back, and the more of them the longer it takes: the share
    // is squared, so four of them make it nine times rarer rather than a fifth rarer.
    const std::int32_t crowded = callsToWeather({{9, 64, 8, stateOf("exposed_copper")},
                                                 {7, 64, 8, stateOf("exposed_copper")},
                                                 {8, 64, 9, stateOf("exposed_copper")},
                                                 {8, 64, 7, stateOf("exposed_copper")}});
    CHECK(crowded > alone * 3);
}

TEST_CASE("what does not weather is left alone") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(2, 64, 2, stateOf("stone")));
    REQUIRE(world.setBlockState(3, 64, 2, stateOf("oxidized_copper")));

    rng::RandomSource random = rng::RandomSource::legacy(7);
    std::vector<BlockChange> changes;
    for (std::int32_t at = 0; at < 2000; ++at) {
        weatheringGrowth::weather(world, 2, 64, 2, *world.blockState(2, 64, 2), random, changes);
        // parity: oxidized is the last stage, so it has nowhere left to go.
        weatheringGrowth::weather(world, 3, 64, 2, *world.blockState(3, 64, 2), random, changes);
    }
    CHECK(changes.empty());
}
