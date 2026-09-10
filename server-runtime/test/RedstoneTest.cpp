#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/ContainerOpeners.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::registry::BlockFace;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> state =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(state.has_value());
    return *state;
}

[[nodiscard]] std::int32_t wireOf(std::int32_t power, const std::string& north = "none",
                                  const std::string& south = "none", const std::string& west = "none",
                                  const std::string& east = "none") {
    return stateOf(
        "redstone_wire",
        {{"power", std::to_string(power)}, {"north", north}, {"south", south}, {"west", west}, {"east", east}});
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::int32_t powerOf(std::int32_t stateId) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find("power");
    return found == properties.end() ? -1 : std::stoi(found->second);
}

} // namespace

TEST_CASE("what each source gives off, and which way") {
    CHECK(redstone::signalOf(stateOf("redstone_block"), BlockFace::Up) == 15);
    CHECK(redstone::signalOf(stateOf("stone"), BlockFace::Up) == 0);

    // parity: a torch feeds every side but the one above it.
    const std::int32_t litTorch = stateOf("redstone_torch", {{"lit", "true"}});
    CHECK(redstone::signalOf(litTorch, BlockFace::Down) == 15);
    CHECK(redstone::signalOf(litTorch, BlockFace::North) == 15);
    CHECK(redstone::signalOf(litTorch, BlockFace::Up) == 0);
    CHECK(redstone::signalOf(stateOf("redstone_torch", {{"lit", "false"}}), BlockFace::North) == 0);

    // parity: a wall torch does not feed the wall it hangs on, which is the side it faces.
    const std::int32_t wallTorch = stateOf("redstone_wall_torch", {{"lit", "true"}, {"facing", "north"}});
    CHECK(redstone::signalOf(wallTorch, BlockFace::South) == 15);
    CHECK(redstone::signalOf(wallTorch, BlockFace::North) == 0);

    // parity: a repeater feeds only the side it faces, and only while it is on.
    const std::int32_t repeater =
        stateOf("repeater", {{"powered", "true"}, {"facing", "east"}, {"delay", "1"}, {"locked", "false"}});
    CHECK(redstone::signalOf(repeater, BlockFace::East) == 15);
    CHECK(redstone::signalOf(repeater, BlockFace::West) == 0);

    // A comparator carries whatever it worked out, which is handed in rather than read off it.
    const std::int32_t comparator =
        stateOf("comparator", {{"powered", "true"}, {"facing", "east"}, {"mode", "compare"}});
    CHECK(redstone::signalOf(comparator, BlockFace::East, 0, 7) == 7);
    CHECK(redstone::signalOf(comparator, BlockFace::West, 0, 7) == 0);

    // A trapped chest counts the people looking inside, and no more than fifteen of them.
    CHECK(redstone::signalOf(stateOf("trapped_chest"), BlockFace::Up, 3) == 3);
    CHECK(redstone::signalOf(stateOf("trapped_chest"), BlockFace::Up, 100) == 15);
    CHECK(redstone::signalOf(stateOf("trapped_chest"), BlockFace::Up, 0) == 0);
}

TEST_CASE("dust feeds the sides it is drawn towards, and the block under it") {
    const std::int32_t dust = wireOf(9, "side", "none", "none", "side");
    // Down is nothing, up is the block it sits on and takes the full strength.
    CHECK(redstone::signalOf(dust, BlockFace::Down) == 0);
    CHECK(redstone::signalOf(dust, BlockFace::Up) == 9);
    // Asked from the south means the dust's north side, which is drawn.
    CHECK(redstone::signalOf(dust, BlockFace::South) == 9);
    // Asked from the north is its south side, which is not.
    CHECK(redstone::signalOf(dust, BlockFace::North) == 0);
    CHECK(redstone::signalOf(dust, BlockFace::West) == 9);
    CHECK(redstone::signalOf(dust, BlockFace::East) == 0);

    // Dust with nothing in it feeds nothing, and muted dust feeds nothing either.
    CHECK(redstone::signalOf(wireOf(0, "side"), BlockFace::Up) == 0);
    CHECK(redstone::signalOf(dust, BlockFace::Up, 0, 15, true) == 0);
}

TEST_CASE("the strong signal that goes through a block") {
    // parity: a lever strongly powers the block it is attached to and nothing else.
    const std::int32_t lever = stateOf("lever", {{"powered", "true"}, {"face", "floor"}, {"facing", "north"}});
    CHECK(redstone::directSignalOf(lever, BlockFace::Up) == 15);
    CHECK(redstone::directSignalOf(lever, BlockFace::Down) == 0);
    CHECK(redstone::directSignalOf(lever, BlockFace::North) == 0);
    // While weakly it feeds every side.
    CHECK(redstone::signalOf(lever, BlockFace::North) == 15);

    // A torch strongly powers the block above it, which is `Down` as the block above asks.
    const std::int32_t torch = stateOf("redstone_torch", {{"lit", "true"}});
    CHECK(redstone::directSignalOf(torch, BlockFace::Down) == 15);
    CHECK(redstone::directSignalOf(torch, BlockFace::North) == 0);

    // A block of redstone is weak everywhere, which is why it does not put out a torch through a
    // block.
    CHECK(redstone::directSignalOf(stateOf("redstone_block"), BlockFace::Up) == 0);

    // A plate strongly powers what it stands on.
    const std::int32_t plate = stateOf("stone_pressure_plate", {{"powered", "true"}});
    CHECK(redstone::directSignalOf(plate, BlockFace::Up) == 15);
    CHECK(redstone::directSignalOf(plate, BlockFace::North) == 0);
}

TEST_CASE("what reaches a block from around it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    CHECK(redstone::bestNeighbourSignal(world, "world", 8, 64, 8) == 0);

    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    CHECK(redstone::bestNeighbourSignal(world, "world", 8, 64, 8) == 15);

    // Through a conductor: a lever on a block strongly powers it, and the block hands that on.
    WorldState through = emptyWorld();
    through.getOrGenerateChunk(0, 0);
    REQUIRE(through.setBlockState(8, 64, 8, stateOf("stone")));
    REQUIRE(through.setBlockState(8, 65, 8,
                                  stateOf("lever", {{"powered", "true"}, {"face", "floor"}, {"facing", "north"}})));
    CHECK(redstone::signalFrom(through, "world", 8, 64, 8, BlockFace::North) == 15);
    // A block of redstone next to the stone is weak, so the stone hands on nothing.
    WorldState weak = emptyWorld();
    weak.getOrGenerateChunk(0, 0);
    REQUIRE(weak.setBlockState(8, 64, 8, stateOf("stone")));
    REQUIRE(weak.setBlockState(9, 64, 8, stateOf("redstone_block")));
    CHECK(redstone::signalFrom(weak, "world", 8, 64, 8, BlockFace::North) == 0);
}

TEST_CASE("a line of dust loses one per block") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t x = 0; x < 12; ++x) {
        REQUIRE(world.setBlockState(x, 63, 8, stateOf("stone")));
        REQUIRE(world.setBlockState(x, 64, 8, wireOf(0, "none", "none", "side", "side")));
    }
    REQUIRE(world.setBlockState(12, 64, 8, stateOf("redstone_block")));

    const std::vector<BlockChange> changes = redstone::settle(world, "world", 12, 64, 8);
    CHECK_FALSE(changes.empty());
    // parity: fifteen beside the source and one less each block, gone after fifteen.
    CHECK(powerOf(*world.blockState(11, 64, 8)) == 15);
    CHECK(powerOf(*world.blockState(10, 64, 8)) == 14);
    CHECK(powerOf(*world.blockState(5, 64, 8)) == 9);
    CHECK(powerOf(*world.blockState(0, 64, 8)) == 4);

    // And taking the source away puts the whole line out.
    REQUIRE(world.setBlockState(12, 64, 8, registry::blocks::air().stateId));
    static_cast<void>(redstone::settle(world, "world", 12, 64, 8));
    for (std::int32_t x = 0; x < 12; ++x) {
        CAPTURE(x);
        CHECK(powerOf(*world.blockState(x, 64, 8)) == 0);
    }
}

TEST_CASE("dust steps up over a block and down off one") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    world.getOrGenerateChunk(-1, 0);
    // A floor, a piece of dust on it, and a step up to the east with dust on top of that step.
    REQUIRE(world.setBlockState(0, 63, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(0, 64, 8, wireOf(0, "none", "none", "side", "side")));
    REQUIRE(world.setBlockState(1, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(1, 65, 8, wireOf(10, "none", "none", "side", "side")));

    // parity: the signal climbs the step, losing the usual one.
    CHECK(redstone::wireStrength(world, "world", 0, 64, 8) == 9);

    // A block over the dust roofs the climb: what is on top of the step cannot be seen at all.
    REQUIRE(world.setBlockState(0, 65, 8, stateOf("stone")));
    CHECK(redstone::wireStrength(world, "world", 0, 64, 8) == 0);
    REQUIRE(world.setBlockState(0, 65, 8, registry::blocks::air().stateId));

    // And a step down: nothing solid to the west, dust one below it.
    REQUIRE(world.setBlockState(-1, 62, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(-1, 63, 8, wireOf(12, "none", "none", "side", "side")));
    CHECK(redstone::wireStrength(world, "world", 0, 64, 8) == 11);

    // Fill the gap and the drop stops being one: dust does not read through a solid block.
    REQUIRE(world.setBlockState(-1, 64, 8, stateOf("stone")));
    CHECK(redstone::wireStrength(world, "world", 0, 64, 8) == 9);
}

TEST_CASE("a block that does not conduct hands nothing on") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: glass carries no signal, so a lever on it powers the lever's own block and no more.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("glass")));
    REQUIRE(
        world.setBlockState(8, 65, 8, stateOf("lever", {{"powered", "true"}, {"face", "floor"}, {"facing", "north"}})));
    CHECK(redstone::signalFrom(world, "world", 8, 64, 8, BlockFace::North) == 0);

    // The same lever on stone does hand it on.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    CHECK(redstone::signalFrom(world, "world", 8, 64, 8, BlockFace::North) == 15);
}

TEST_CASE("every piece of dust that moved is reported once") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    for (std::int32_t x = 0; x < 4; ++x) {
        REQUIRE(world.setBlockState(x, 63, 8, stateOf("stone")));
        REQUIRE(world.setBlockState(x, 64, 8, wireOf(0, "none", "none", "side", "side")));
    }
    REQUIRE(world.setBlockState(4, 64, 8, stateOf("redstone_block")));

    std::vector<std::array<std::int32_t, 3>> told;
    const std::vector<BlockChange> changes =
        redstone::settle(world, "world", 4, 64, 8,
                         [&told](std::int32_t x, std::int32_t y, std::int32_t z) { told.push_back({x, y, z}); });
    CHECK(changes.size() == 4);
    // Whoever schedules the components hears about each one of them, and only the ones that moved.
    CHECK(told.size() == changes.size());
}

TEST_CASE("what a comparator reads off a container") {
    // parity: one item of any container already reads one, and a full one reads fifteen.
    const std::vector<std::pair<std::int32_t, std::int32_t>> one{{1, 64}};
    CHECK(redstone::containerSignal(one, 27) == 1);
    CHECK(redstone::containerSignal(one, 3) == 1);
    CHECK(redstone::containerSignal({}, 27) == 0);

    std::vector<std::pair<std::int32_t, std::int32_t>> full;
    for (std::int32_t slot = 0; slot < 27; ++slot) {
        full.emplace_back(64, 64);
    }
    CHECK(redstone::containerSignal(full, 27) == 15);

    // Half full reads seven, which is the number every hopper clock is built on.
    std::vector<std::pair<std::int32_t, std::int32_t>> half;
    for (std::int32_t slot = 0; slot < 14; ++slot) {
        half.emplace_back(64, 64);
    }
    CHECK(redstone::containerSignal(half, 27) == 8);
    CHECK(redstone::containerSignal(one, 0) == 0);
}

TEST_CASE("which blocks can feed a circuit at all") {
    CHECK(redstone::isSignalSource(stateOf("redstone_block")));
    CHECK(redstone::isSignalSource(stateOf("lever")));
    CHECK(redstone::isSignalSource(stateOf("stone_button")));
    CHECK(redstone::isSignalSource(stateOf("oak_pressure_plate")));
    CHECK(redstone::isSignalSource(stateOf("observer")));
    CHECK(redstone::isSignalSource(stateOf("trapped_chest")));
    CHECK_FALSE(redstone::isSignalSource(stateOf("chest")));
    CHECK_FALSE(redstone::isSignalSource(stateOf("stone")));
    // parity: `RedStoneWireBlock.isSignalSource` is true as well. Easy to miss, because every redstone
    // path checks for the dust before it gets round to asking: the two that do ask are the plain rail
    // turning a corner and `NaturalSpawner.isValidEmptySpawnBlock`, which is what stops a mob spawning
    // on a piece of dust.
    CHECK(redstone::isSignalSource(stateOf("redstone_wire")));

    CHECK(redstone::isWire(stateOf("redstone_wire")));
    CHECK_FALSE(redstone::isWire(stateOf("redstone_block")));
}

TEST_CASE("the reach of a change is two blocks through anything solid") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Nothing around: the block itself and its six neighbours.
    CHECK(redstone::reachOf(world, 8, 64, 8).size() == 7);

    // A conductor beside it adds its own six.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    CHECK(redstone::reachOf(world, 8, 64, 8).size() == 13);
    // Something that does not conduct adds nothing.
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("glass")));
    CHECK(redstone::reachOf(world, 8, 64, 8).size() == 13);
}

TEST_CASE("a trapped chest with somebody looking in it") {
    containerOpeners::clear();
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("trapped_chest")));
    CHECK(redstone::signalFrom(world, "world", 8, 64, 8, BlockFace::Up) == 0);

    static_cast<void>(containerOpeners::open("world", 8, 64, 8));
    static_cast<void>(containerOpeners::open("world", 8, 64, 8));
    CHECK(redstone::signalFrom(world, "world", 8, 64, 8, BlockFace::Up) == 2);
    // Another world's chest of the same coordinates is not this one.
    CHECK(redstone::signalFrom(world, "nether", 8, 64, 8, BlockFace::Up) == 0);
    containerOpeners::clear();
}
