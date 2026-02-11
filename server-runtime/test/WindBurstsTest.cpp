#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/WindBursts.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& block, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(block), properties, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string propertyAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const std::string& key) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// The gust let off in the middle of the square the block is in.
std::vector<BlockChange> gustAt(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    basalt::server::random::RandomSource random = basalt::server::random::RandomSource::legacy(1234);
    return windBursts::burst(world, -64, 319, x + 0.5, y + 0.5, z + 0.5, 0, random);
}

} // namespace

TEST_CASE("the gust opens a wooden door by its lower half and leaves an iron one shut") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::map<std::string, std::string> lower{
        {"facing", "east"}, {"half", "lower"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}};
    const std::map<std::string, std::string> upper{
        {"facing", "east"}, {"half", "upper"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}};
    REQUIRE(world.setBlockState(4, 64, 8, stateOf("oak_door", lower)));
    REQUIRE(world.setBlockState(4, 65, 8, stateOf("oak_door", upper)));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("iron_door", lower)));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("iron_door", upper)));

    CHECK_FALSE(gustAt(world, 4, 64, 8).empty());
    // Both halves move together, which is what keeps a door from coming apart.
    CHECK(propertyAt(world, 4, 64, 8, "open") == "true");
    CHECK(propertyAt(world, 4, 65, 8, "open") == "true");

    // parity: `BlockSetType.canOpenByWindCharge` is false only for iron.
    CHECK(gustAt(world, 8, 64, 8).empty());
    CHECK(propertyAt(world, 8, 64, 8, "open") == "false");
}

TEST_CASE("a door held by a signal does not hear the gust, and neither does its upper half alone") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::map<std::string, std::string> lower{
        {"facing", "east"}, {"half", "lower"}, {"hinge", "left"}, {"open", "false"}, {"powered", "true"}};
    const std::map<std::string, std::string> upper{
        {"facing", "east"}, {"half", "upper"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}};
    REQUIRE(world.setBlockState(4, 64, 8, stateOf("oak_door", lower)));
    REQUIRE(world.setBlockState(4, 65, 8, stateOf("oak_door", upper)));

    CHECK(gustAt(world, 4, 64, 8).empty());
    CHECK(propertyAt(world, 4, 64, 8, "open") == "false");

    // parity: `DoorBlock.onExplosionHit` answers only on the lower half, so a gust that reaches the
    // top of a door and nothing else leaves it shut.
    WorldState tall = emptyWorld();
    tall.getOrGenerateChunk(0, 0);
    const std::map<std::string, std::string> free{
        {"facing", "east"}, {"half", "upper"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}};
    REQUIRE(tall.setBlockState(4, 80, 8, stateOf("oak_door", free)));
    CHECK(gustAt(tall, 4, 80, 8).empty());
}

TEST_CASE("the gust pulls a lever whichever way it was, and puts a candle out") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(
        world.setBlockState(4, 64, 8, stateOf("lever", {{"face", "floor"}, {"facing", "north"}, {"powered", "true"}})));
    REQUIRE(world.setBlockState(8, 64, 8,
                                stateOf("candle", {{"candles", "1"}, {"lit", "true"}, {"waterlogged", "false"}})));

    // parity: `LeverBlock.onExplosionHit` has no `powered` guard: a lever that is on goes off.
    CHECK_FALSE(gustAt(world, 4, 64, 8).empty());
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "false");

    CHECK_FALSE(gustAt(world, 8, 64, 8).empty());
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "false");
}

TEST_CASE("a button the gust presses books its own release") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(4, 64, 8,
                                stateOf("oak_button", {{"face", "wall"}, {"facing", "east"}, {"powered", "false"}})));

    CHECK_FALSE(gustAt(world, 4, 64, 8).empty());
    CHECK(propertyAt(world, 4, 64, 8, "powered") == "true");
    CHECK(world.hasScheduledBlockTick(4, 64, 8, Identifier::vanilla("oak_button")));

    // parity: every `onExplosionHit` but the lever's refuses while the block is already down.
    CHECK(gustAt(world, 4, 64, 8).empty());
}
