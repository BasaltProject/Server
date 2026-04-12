#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/SculkSpreading.hpp>
#include <basalt/server/runtime/world/WorldState.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
using basalt::wire::nbt::NbtCompound;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties = {}) {
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

struct Loaded {
    WorldState world{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};

    Loaded() {
        for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                world.getOrGenerateChunk(chunkX, chunkZ);
            }
        }
    }
};

} // namespace

TEST_CASE("cursors survive the block entity in vanilla's own shape") {
    std::vector<sculkSpreading::Cursor> cursors;
    sculkSpreading::addCursors(cursors, {1, 2, 3}, 2500);
    REQUIRE(cursors.size() == 3);
    CHECK(cursors[0].charge == 1000);
    CHECK(cursors[2].charge == 500);
    cursors[1].facings = 0b000011;
    cursors[1].updateDelay = 1;
    NbtCompound data;
    sculkSpreading::write(cursors, data);
    const std::vector<sculkSpreading::Cursor> read = sculkSpreading::read(data);
    REQUIRE(read.size() == 3);
    CHECK(read[1].pos == std::array<std::int32_t, 3>{1, 2, 3});
    CHECK(read[1].facings == std::optional<std::uint8_t>{0b000011});
    CHECK(read[1].updateDelay == 1);
    CHECK_FALSE(read[0].facings.has_value());
    // Past the thirty-two a spreader keeps, the saved list is not read at all.
    std::vector<sculkSpreading::Cursor> many(33, sculkSpreading::Cursor{.charge = 1});
    sculkSpreading::write(many, data);
    CHECK(sculkSpreading::read(data).empty());
}

TEST_CASE("a charged vein turns the block it clings to into sculk") {
    Loaded loaded;
    WorldState& world = loaded.world;
    REQUIRE(world.setBlockState(0, 64, 0, stateOf("stone")));
    REQUIRE(world.setBlockState(0, 65, 0, stateOf("sculk_vein", {{"down", "true"}})));
    std::vector<sculkSpreading::Cursor> cursors{
        sculkSpreading::Cursor{.pos = {0, 65, 0}, .charge = 5, .facings = std::uint8_t{1}}};
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(7);
    std::vector<sculkSpreading::Step> steps;
    const sculkSpreading::BlockTagged tagged = [](std::string_view tag, std::int32_t state) {
        return tag == "minecraft:sculk_replaceable" && state == stateOf("stone");
    };
    sculkSpreading::update(world, cursors, {10, 64, 10}, random, tagged, steps);
    CHECK(world.blockState(0, 64, 0) == std::optional{stateOf("sculk")});
    REQUIRE(cursors.size() == 1);
    CHECK(cursors.front().charge == 4);
    CHECK_FALSE(steps.empty());
}
