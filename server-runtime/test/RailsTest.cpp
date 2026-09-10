#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Rails.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t railOf(const std::string& path, const std::string& shape) {
    std::map<std::string, std::string> properties{{"shape", shape}};
    if (path != "rail") {
        properties.emplace("powered", "false");
        properties.emplace("waterlogged", "false");
    } else {
        properties.emplace("waterlogged", "false");
    }
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::string shapeAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    if (!stateId.has_value()) {
        return {};
    }
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find("shape");
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

} // namespace

TEST_CASE("which blocks are rails") {
    CHECK(rails::isRail(stateOf("rail")));
    CHECK(rails::isRail(stateOf("powered_rail")));
    CHECK(rails::isRail(stateOf("detector_rail")));
    CHECK(rails::isRail(stateOf("activator_rail")));
    CHECK_FALSE(rails::isRail(stateOf("stone")));
    CHECK_FALSE(rails::isRail(registry::blocks::air().stateId));
}

TEST_CASE("a rail laid beside another turns to meet it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "north_south")));

    // The new one runs east to west because that is where its neighbour is.
    const std::optional<std::int32_t> placed = rails::place(world, "world", 9, 64, 8);
    REQUIRE(placed.has_value());
    CHECK(shapeAt(world, 9, 64, 8) == "east_west");
    // And the one it met was bent towards it.
    CHECK(shapeAt(world, 8, 64, 8) == "east_west");
}

TEST_CASE("a plain rail turns a corner and a powered one does not") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A rail to the north and one to the east of the position being laid.
    REQUIRE(world.setBlockState(8, 64, 7, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));
    static_cast<void>(rails::place(world, "world", 8, 64, 8));
    CHECK(shapeAt(world, 8, 64, 8) == "north_east");

    // The same corner with a powered rail: it can only run straight.
    WorldState powered = emptyWorld();
    powered.getOrGenerateChunk(0, 0);
    REQUIRE(powered.setBlockState(8, 64, 7, railOf("rail", "north_south")));
    REQUIRE(powered.setBlockState(9, 64, 8, railOf("rail", "east_west")));
    REQUIRE(powered.setBlockState(8, 64, 8, railOf("powered_rail", "north_south")));
    static_cast<void>(rails::place(powered, "world", 8, 64, 8));
    const std::string shape = shapeAt(powered, 8, 64, 8);
    CHECK((shape == "north_south" || shape == "east_west"));
}

TEST_CASE("a rail climbs towards whichever side is higher") {
    // parity: `RailState.place`'s slope, one case per direction.
    const auto slopeTowards = [](std::int32_t stepX, std::int32_t stepZ, const std::string& start) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8 + stepX, 65, 8 + stepZ, railOf("rail", "north_south")));
        REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", start)));
        static_cast<void>(rails::place(world, "world", 8, 64, 8));
        return shapeAt(world, 8, 64, 8);
    };
    CHECK(slopeTowards(0, -1, "north_south") == "ascending_north");
    CHECK(slopeTowards(0, 1, "north_south") == "ascending_south");
    CHECK(slopeTowards(1, 0, "east_west") == "ascending_east");
    CHECK(slopeTowards(-1, 0, "east_west") == "ascending_west");
}

TEST_CASE("a rail finds its neighbour a block above or a block below") {
    // parity: `getRail` looks at the block, then one up, then one down, which is what lets a track
    // run over a hill without a break.
    const auto shapeWithNeighbourAt = [](std::int32_t offsetY) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64 + offsetY, 7, railOf("rail", "north_south")));
        // Starting east to west, so only a neighbour it can see turns it.
        REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "east_west")));
        static_cast<void>(rails::place(world, "world", 8, 64, 8));
        return shapeAt(world, 8, 64, 8);
    };
    CHECK(shapeWithNeighbourAt(0) == "north_south");
    CHECK(shapeWithNeighbourAt(-1) == "north_south");
    // One above turns it into a slope, which is the same connection seen from below.
    CHECK(shapeWithNeighbourAt(1) == "ascending_north");
    // Two below is out of reach: it stays as it was laid.
    CHECK(shapeWithNeighbourAt(-2) == "east_west");
}

TEST_CASE("a rail joined at both ends takes no more") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A straight run of three, the middle one joined north and south.
    REQUIRE(world.setBlockState(8, 64, 6, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(8, 64, 7, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));
    static_cast<void>(rails::place(world, "world", 8, 64, 7));
    REQUIRE(shapeAt(world, 8, 64, 7) == "north_south");

    // A rail laid to the east of the middle one: it is full, so the new rail does not connect to it
    // and the middle one is not bent towards it either.
    REQUIRE(world.setBlockState(9, 64, 7, railOf("rail", "north_south")));
    static_cast<void>(rails::place(world, "world", 9, 64, 7));
    CHECK(shapeAt(world, 8, 64, 7) == "north_south");
    CHECK(shapeAt(world, 9, 64, 7) == "north_south");
}

TEST_CASE("a rail with a free end takes a new neighbour whichever way it points") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `canConnectTo` counts the connections a rail really has, not the ones its shape
    // claims: the rail to the north runs east to west with nothing at either end, so it is free and
    // the new rail turns towards it.
    REQUIRE(world.setBlockState(8, 64, 7, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));
    static_cast<void>(rails::place(world, "world", 8, 64, 8));
    CHECK(shapeAt(world, 8, 64, 8) == "north_east");
}

TEST_CASE("what is not a rail, and what is not there") {
    WorldState world = emptyWorld();
    CHECK_FALSE(rails::place(world, "world", 0, 64, 0).has_value());

    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(0, 64, 0, stateOf("stone")));
    CHECK_FALSE(rails::place(world, "world", 0, 64, 0).has_value());
}

TEST_CASE("every rail that was bent is reported") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "north_south")));

    std::vector<std::array<std::int32_t, 3>> told;
    static_cast<void>(
        rails::place(world, "world", 9, 64, 8, [&told](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t) {
            told.push_back({x, y, z});
        }));
    // The rail laid and the one it met.
    CHECK(told.size() == 2);
    CHECK(told[0] == std::array<std::int32_t, 3>{9, 64, 8});
    CHECK(told[1] == std::array<std::int32_t, 3>{8, 64, 8});
}

TEST_CASE("a rail only re-decides on a signal, and only at a junction") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A T: rails to the north, the south and the east of the middle one.
    REQUIRE(world.setBlockState(8, 64, 7, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(8, 64, 9, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));

    // Something that gives off no signal changes nothing, junction or not.
    CHECK_FALSE(rails::onNeighbourChange(world, "world", 8, 64, 8, stateOf("stone")).has_value());
    CHECK(shapeAt(world, 8, 64, 8) == "north_south");

    // A lever beside it, and now the junction re-decides.
    const std::optional<std::int32_t> lever = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("lever"), {{"powered", "true"}, {"face", "floor"}, {"facing", "north"}}, std::nullopt);
    REQUIRE(lever.has_value());
    REQUIRE(world.setBlockState(7, 64, 8, *lever));
    const std::optional<std::int32_t> turned = rails::onNeighbourChange(world, "world", 8, 64, 8, *lever);
    REQUIRE(turned.has_value());
    CHECK(shapeAt(world, 8, 64, 8) != "north_south");

    // Asked again with nothing changed, it says nothing rather than reporting the shape it is
    // already in.
    CHECK_FALSE(rails::onNeighbourChange(world, "world", 8, 64, 8, *lever).has_value());
}

TEST_CASE("a rail joined at both ends still takes the one it already points at") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `canConnectTo` answers yes to whoever it is already joined to, however full it is.
    // The rail to the north runs between the one beyond it and this one, so it has both its
    // connections and one of them is us.
    REQUIRE(world.setBlockState(8, 64, 6, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(8, 64, 7, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));

    static_cast<void>(rails::place(world, "world", 8, 64, 8));
    CHECK(shapeAt(world, 8, 64, 8) == "north_east");
}

TEST_CASE("a rail full of other connections is not bent towards a new one") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // The rail to the east runs north to south between two rails of its own: it is full, and
    // neither of those connections is the rail being laid.
    REQUIRE(world.setBlockState(9, 64, 7, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 9, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "north_south")));
    // A free rail to the west, so the new one runs east to west and reaches towards the full one.
    REQUIRE(world.setBlockState(7, 64, 8, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "east_west")));

    static_cast<void>(rails::place(world, "world", 8, 64, 8));
    CHECK(shapeAt(world, 8, 64, 8) == "east_west");
    // It kept its own run rather than turning towards what was laid beside it.
    CHECK(shapeAt(world, 9, 64, 8) == "north_south");
}

TEST_CASE("a rail pointing at rails that do not point back is free") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `removeSoftConnections`. The rail to the north points east and west at two rails that
    // run the other way, so neither of them is really a connection and it is free to take a third.
    REQUIRE(world.setBlockState(7, 64, 7, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(9, 64, 7, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(8, 64, 7, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(9, 64, 8, railOf("rail", "east_west")));
    REQUIRE(world.setBlockState(8, 64, 8, railOf("rail", "north_south")));

    static_cast<void>(rails::place(world, "world", 8, 64, 8));
    CHECK(shapeAt(world, 8, 64, 8) == "north_east");
}

TEST_CASE("only a plain rail at a real junction re-decides") {
    const std::optional<std::int32_t> lever = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("lever"), {{"powered", "true"}, {"face", "floor"}, {"facing", "north"}}, std::nullopt);
    REQUIRE(lever.has_value());

    // A junction where the answer would differ: three rails around it and the northern one a block
    // higher, so a rail that re-decided would become a slope.
    // Rails north and south, and one to the east a block higher: three ways to go, and a shape
    // that would change if the rail were re-decided.
    const auto junction = [&lever](const std::string& path, const std::string& start) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 7, railOf("rail", "north_south")));
        REQUIRE(world.setBlockState(8, 64, 9, railOf("rail", "north_south")));
        REQUIRE(world.setBlockState(9, 65, 8, railOf("rail", "east_west")));
        REQUIRE(world.setBlockState(8, 64, 8, railOf(path, start)));
        REQUIRE(world.setBlockState(7, 64, 8, *lever));
        const std::optional<std::int32_t> turned = rails::onNeighbourChange(world, "world", 8, 64, 8, *lever);
        return turned.has_value() ? shapeAt(world, 8, 64, 8) : std::string{"unchanged"};
    };
    CHECK(junction("rail", "north_south") == "north_east");
    // parity: `BaseRailBlock.updateState` is empty for the three that carry a signal, so the same
    // junction leaves them exactly as they were even though their shape would otherwise climb.
    CHECK(junction("powered_rail", "east_west") == "unchanged");
    CHECK(junction("detector_rail", "east_west") == "unchanged");

    // And a straight run is not a junction: two neighbours are not three, however much the shape
    // would otherwise change.
    WorldState line = emptyWorld();
    line.getOrGenerateChunk(0, 0);
    REQUIRE(line.setBlockState(8, 64, 7, railOf("rail", "north_south")));
    REQUIRE(line.setBlockState(8, 64, 9, railOf("rail", "north_south")));
    REQUIRE(line.setBlockState(8, 64, 8, railOf("rail", "east_west")));
    REQUIRE(line.setBlockState(7, 64, 8, *lever));
    CHECK_FALSE(rails::onNeighbourChange(line, "world", 8, 64, 8, *lever).has_value());
    CHECK(shapeAt(line, 8, 64, 8) == "east_west");
}

TEST_CASE("a rail put down beside a rail one block lower climbs down to meet it") {
    // parity: `RailState.hasRail` looks at the square, the one above it and the one below, so a rail
    // laid on a step joins the one at the bottom of it: the lower one turns into the ramp and the
    // upper one takes the axis that points at it.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(5, 64, 2, railOf("rail", "north_south")));
    REQUIRE(world.setBlockState(6, 64, 2, stateOf("stone")));
    REQUIRE(world.setBlockState(6, 65, 2, railOf("rail", "north_south")));

    static_cast<void>(rails::place(world, {}, 6, 65, 2));

    CHECK(shapeAt(world, 6, 65, 2) == "east_west");
    CHECK(shapeAt(world, 5, 64, 2) == "ascending_east");
}
