#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Redstone.hpp>
#include <basalt/server/runtime/world/RedstoneComponents.hpp>

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

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& key) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::string propertyAt(const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                     const std::string& key) {
    const std::optional<std::int32_t> stateId = world.blockState(x, y, z);
    return stateId.has_value() ? propertyOf(*stateId, key) : std::string{};
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

[[nodiscard]] std::int32_t repeaterState(const std::string& facing, const std::string& delay,
                                         const std::string& powered, const std::string& locked = "false") {
    return stateOf("repeater", {{"facing", facing}, {"delay", delay}, {"powered", powered}, {"locked", locked}});
}

[[nodiscard]] std::int32_t comparatorState(const std::string& facing, const std::string& mode,
                                           const std::string& powered) {
    return stateOf("comparator", {{"facing", facing}, {"mode", mode}, {"powered", powered}});
}

} // namespace

TEST_CASE("which blocks take a turn of their own") {
    CHECK(redstoneComponents::isComponent(stateOf("redstone_torch")));
    CHECK(redstoneComponents::isComponent(stateOf("redstone_wall_torch")));
    CHECK(redstoneComponents::isComponent(stateOf("repeater")));
    CHECK(redstoneComponents::isComponent(stateOf("comparator")));
    CHECK(redstoneComponents::isComponent(stateOf("observer")));
    CHECK_FALSE(redstoneComponents::isComponent(stateOf("redstone_wire")));
    CHECK_FALSE(redstoneComponents::isComponent(stateOf("stone")));
}

TEST_CASE("a torch goes out when the block it stands on is powered") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("redstone_torch", {{"lit", "true"}})));

    // Nothing feeding the stone: the torch is lit and stays lit.
    CHECK_FALSE(redstoneComponents::tick(world, "world", 8, 65, 8).has_value());
    CHECK(propertyAt(world, 8, 65, 8, "lit") == "true");

    // A lever on the stone strongly powers it, so the torch goes out on its turn.
    REQUIRE(world.setBlockState(8, 63, 8,
                                stateOf("lever", {{"powered", "true"}, {"face", "ceiling"}, {"facing", "north"}})));
    const std::optional<BlockChange> out = redstoneComponents::tick(world, "world", 8, 65, 8);
    REQUIRE(out.has_value());
    CHECK(propertyOf(out->stateId, "lit") == "false");

    // And it comes back when the lever does.
    REQUIRE(world.setBlockState(8, 63, 8,
                                stateOf("lever", {{"powered", "false"}, {"face", "ceiling"}, {"facing", "north"}})));
    const std::optional<BlockChange> relit = redstoneComponents::tick(world, "world", 8, 65, 8);
    REQUIRE(relit.has_value());
    CHECK(propertyOf(relit->stateId, "lit") == "true");
}

TEST_CASE("a wall torch reads the wall behind it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: a wall torch facing east hangs on the block to its west.
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("redstone_wall_torch", {{"lit", "true"}, {"facing", "east"}})));
    CHECK_FALSE(redstoneComponents::tick(world, "world", 8, 64, 8).has_value());

    REQUIRE(
        world.setBlockState(7, 65, 8, stateOf("lever", {{"powered", "true"}, {"face", "floor"}, {"facing", "north"}})));
    const std::optional<BlockChange> out = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(out.has_value());
    CHECK(propertyOf(out->stateId, "lit") == "false");
}

TEST_CASE("a repeater turns on from what it faces and books its own turn off") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // A repeater facing east reads the block to its east.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    REQUIRE(world.setBlockState(8, 64, 8, repeaterState("east", "1", "false")));

    const std::optional<BlockChange> on = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(on.has_value());
    CHECK(propertyOf(on->stateId, "powered") == "true");
    // Already on: nothing to report.
    CHECK_FALSE(redstoneComponents::tick(world, "world", 8, 64, 8).has_value());

    // The source goes and the repeater turns off, booking a turn as it does.
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    const std::optional<BlockChange> off = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(off.has_value());
    CHECK(propertyOf(off->stateId, "powered") == "false");
    CHECK(world.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("repeater")));
}

TEST_CASE("a repeater held by another one does not move") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    REQUIRE(world.setBlockState(8, 64, 8, repeaterState("east", "1", "false")));
    // parity: `RepeaterBlock.isLocked`. A powered repeater pointing at this one from the side holds
    // it exactly where it is, which is what a redstone latch is built out of.
    REQUIRE(world.setBlockState(8, 64, 7, repeaterState("south", "1", "true")));

    CHECK_FALSE(redstoneComponents::tick(world, "world", 8, 64, 8).has_value());
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "false");

    // A repeater beside it that is not powered locks nothing.
    REQUIRE(world.setBlockState(8, 64, 7, repeaterState("south", "1", "false")));
    CHECK(redstoneComponents::tick(world, "world", 8, 64, 8).has_value());

    // And one pointing the other way locks nothing either.
    REQUIRE(world.setBlockState(8, 64, 8, repeaterState("east", "1", "false")));
    REQUIRE(world.setBlockState(8, 64, 7, repeaterState("north", "1", "true")));
    CHECK(redstoneComponents::tick(world, "world", 8, 64, 8).has_value());
}

TEST_CASE("an observer pulses on and then off by itself") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("observer", {{"facing", "east"}, {"powered", "false"}})));

    const std::optional<BlockChange> on = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(on.has_value());
    CHECK(propertyOf(on->stateId, "powered") == "true");
    // It booked its own turn to go off again: that is the two-tick blink.
    CHECK(world.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("observer")));

    const std::optional<BlockChange> off = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(off.has_value());
    CHECK(propertyOf(off->stateId, "powered") == "false");
}

TEST_CASE("an observer only watches the block it faces") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Facing east, so it watches the block at x + 1.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("observer", {{"facing", "east"}, {"powered", "false"}})));

    redstoneComponents::watchFor(world, 9, 64, 8);
    CHECK(world.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("observer")));

    // Anything else moving beside it is ignored.
    WorldState other = emptyWorld();
    other.getOrGenerateChunk(0, 0);
    REQUIRE(other.setBlockState(8, 64, 8, stateOf("observer", {{"facing", "east"}, {"powered", "false"}})));
    redstoneComponents::watchFor(other, 7, 64, 8);
    redstoneComponents::watchFor(other, 8, 65, 8);
    CHECK_FALSE(other.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("observer")));
}

TEST_CASE("a comparator compares its rear input against its sides") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Facing east: it reads the block to its east and puts out to the west.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    REQUIRE(world.setBlockState(8, 64, 8, comparatorState("east", "compare", "false")));

    const std::optional<BlockChange> on = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(on.has_value());
    CHECK(propertyOf(on->stateId, "powered") == "true");
    // The strength it settled on is kept where a reader looks for it.
    CHECK(redstone::comparatorOutputAt(world, 8, 64, 8) == 15);

    // parity: a block of redstone at the side is not a side input at all. Only a diode or dust
    // counts, which is why a lever or a block beside a comparator does nothing to it.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("redstone_block")));
    CHECK_FALSE(redstoneComponents::tick(world, "world", 8, 64, 8).has_value());
    CHECK(redstone::comparatorOutputAt(world, 8, 64, 8) == 15);
}

TEST_CASE("a side that beats the rear puts a comparator out") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // Five behind it and nine at the side: the sides win, so it gives off nothing at all.
    REQUIRE(world.setBlockState(
        9, 64, 8,
        stateOf("redstone_wire",
                {{"power", "5"}, {"north", "none"}, {"south", "none"}, {"west", "side"}, {"east", "none"}})));
    REQUIRE(world.setBlockState(
        8, 64, 7,
        stateOf("redstone_wire",
                {{"power", "9"}, {"north", "none"}, {"south", "side"}, {"west", "none"}, {"east", "none"}})));
    REQUIRE(world.setBlockState(8, 64, 8, comparatorState("east", "compare", "true")));

    const std::optional<BlockChange> out = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(out.has_value());
    CHECK(propertyOf(out->stateId, "powered") == "false");
    CHECK(redstone::comparatorOutputAt(world, 8, 64, 8) == 0);

    // With the side weaker than the rear it comes back on, carrying the rear's own strength.
    REQUIRE(world.setBlockState(
        8, 64, 7,
        stateOf("redstone_wire",
                {{"power", "2"}, {"north", "none"}, {"south", "side"}, {"west", "none"}, {"east", "none"}})));
    const std::optional<BlockChange> on = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(on.has_value());
    CHECK(propertyOf(on->stateId, "powered") == "true");
    CHECK(redstone::comparatorOutputAt(world, 8, 64, 8) == 5);

    // parity: `getControlInputSignal` counts only a diode or dust at the side. A block of redstone
    // there is full strength and still counts for nothing, so the five behind still wins.
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("redstone_block")));
    CHECK_FALSE(redstoneComponents::tick(world, "world", 8, 64, 8).has_value());
    CHECK(redstone::comparatorOutputAt(world, 8, 64, 8) == 5);
}

TEST_CASE("a repeater reads the dust in front of it even when it points elsewhere") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `DiodeBlock.getInputSignal` reads the dust's own strength, not only what the dust
    // feeds that side: a line running past the nose of a repeater still drives it.
    REQUIRE(world.setBlockState(
        9, 64, 8,
        stateOf("redstone_wire",
                {{"power", "9"}, {"north", "side"}, {"south", "side"}, {"west", "none"}, {"east", "none"}})));
    REQUIRE(world.setBlockState(8, 64, 8, repeaterState("east", "1", "false")));

    // The dust gives that side nothing, and the repeater turns on all the same.
    CHECK(redstone::signalOf(*world.blockState(9, 64, 8), BlockFace::East) == 0);
    const std::optional<BlockChange> on = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(on.has_value());
    CHECK(propertyOf(on->stateId, "powered") == "true");
}

TEST_CASE("subtract mode takes the side off the rear") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    // A piece of dust on the side, carrying nine.
    REQUIRE(world.setBlockState(
        8, 64, 7,
        stateOf("redstone_wire",
                {{"power", "9"}, {"north", "none"}, {"south", "side"}, {"west", "none"}, {"east", "none"}})));
    REQUIRE(world.setBlockState(8, 64, 8, comparatorState("east", "subtract", "false")));

    const std::optional<BlockChange> on = redstoneComponents::tick(world, "world", 8, 64, 8);
    REQUIRE(on.has_value());
    CHECK(propertyOf(on->stateId, "powered") == "true");
    // Fifteen behind and nine at the side leaves six.
    CHECK(redstone::comparatorOutputAt(world, 8, 64, 8) == 6);

    // In compare mode the same pair leaves the rear untouched.
    REQUIRE(world.setBlockState(8, 64, 8, comparatorState("east", "compare", "false")));
    static_cast<void>(redstoneComponents::tick(world, "world", 8, 64, 8));
    CHECK(redstone::comparatorOutputAt(world, 8, 64, 8) == 15);
}

TEST_CASE("a comparator reads a container behind it, through a solid block if it has to") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, comparatorState("east", "compare", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));

    // Nothing knows about containers: the comparator reads the plain signal, which is nothing.
    const std::map<std::string, std::string> properties =
        registry::blockStateDefinitions::propertiesOf(*world.blockState(8, 64, 8));
    CHECK(redstoneComponents::comparatorInput(world, "world", 8, 64, 8, properties) == 0);

    // A container one block further, behind the stone: vanilla reads it through the block.
    const redstoneComponents::ContainerSignalAt containers = [](std::int32_t x, std::int32_t y, std::int32_t z,
                                                                std::string_view) -> std::optional<std::int32_t> {
        if (x == 10 && y == 64 && z == 8) {
            return 4;
        }
        return std::nullopt;
    };
    CHECK(redstoneComponents::comparatorInput(world, "world", 8, 64, 8, properties, containers) == 4);

    // With the container right behind it, that is what it reads and it looks no further.
    const redstoneComponents::ContainerSignalAt behind = [](std::int32_t x, std::int32_t, std::int32_t,
                                                            std::string_view) -> std::optional<std::int32_t> {
        return x == 9 ? std::optional<std::int32_t>{7} : std::nullopt;
    };
    CHECK(redstoneComponents::comparatorInput(world, "world", 8, 64, 8, properties, behind) == 7);
}

TEST_CASE("a change asks the components around it to look again") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(9, 64, 8, repeaterState("east", "3", "false")));
    // parity: `RedstoneTorchBlock.neighborChanged` books a turn only while the torch says the same
    // thing as what it stands on. An unlit torch on an unpowered block is one that owes a turn; a
    // lit one on the same block is right as it is and books nothing.
    REQUIRE(world.setBlockState(7, 64, 8, stateOf("redstone_torch", {{"lit", "false"}})));

    redstoneComponents::scheduleAround(world, "overworld", 8, 64, 8);
    CHECK(world.hasScheduledBlockTick(9, 64, 8, Identifier::vanilla("repeater")));
    CHECK(world.hasScheduledBlockTick(7, 64, 8, Identifier::vanilla("redstone_torch")));

    REQUIRE(world.setBlockState(7, 64, 8, stateOf("redstone_torch", {{"lit", "true"}})));
    WorldState quiet = emptyWorld();
    quiet.getOrGenerateChunk(0, 0);
    REQUIRE(quiet.setBlockState(8, 64, 8, stateOf("stone")));
    REQUIRE(quiet.setBlockState(7, 64, 8, stateOf("redstone_torch", {{"lit", "true"}})));
    redstoneComponents::scheduleAround(quiet, "overworld", 8, 64, 8);
    CHECK_FALSE(quiet.hasScheduledBlockTick(7, 64, 8, Identifier::vanilla("redstone_torch")));

    // parity: a repeater's delay is its own, two ticks per notch, so the three-tick one waits six.
    for (std::int32_t at = 0; at < 5; ++at) {
        const std::vector<std::array<std::int32_t, 3>> due = world.dueBlockTicks();
        // The torch comes due on its second tick and the repeater has not yet.
        if (at == 1) {
            CHECK(due.size() == 1);
        }
    }
    CHECK(world.dueBlockTicks().size() == 1);
}
