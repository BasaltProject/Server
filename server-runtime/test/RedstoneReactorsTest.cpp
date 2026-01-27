#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/RedstoneReactors.hpp>
#include <basalt/server/runtime/world/Vibrations.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

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

[[nodiscard]] std::string soundOf(const BlockChange& change) {
    for (const BlockEffect& effect : change.effects) {
        if (const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&effect); sound != nullptr) {
            return sound->name;
        }
    }
    return {};
}

} // namespace

TEST_CASE("a door opens from a signal at either half and both halves follow") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::map<std::string, std::string> lower{
        {"half", "lower"}, {"facing", "north"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}};
    const std::map<std::string, std::string> upper{
        {"half", "upper"}, {"facing", "north"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}};
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("oak_door", lower)));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("oak_door", upper)));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    // The signal reaches the upper half only, and the whole door still opens.
    REQUIRE(world.setBlockState(9, 65, 8, stateOf("redstone_block")));
    const std::vector<BlockChange> changes = redstoneReactors::update(world, "world", 9, 65, 8, 1, random);
    CHECK_FALSE(changes.empty());
    CHECK(propertyAt(world, 8, 65, 8, "open") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "open") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "true");

    // And it closes again when the signal goes.
    REQUIRE(world.setBlockState(9, 65, 8, registry::blocks::air().stateId));
    static_cast<void>(redstoneReactors::update(world, "world", 9, 65, 8, 1, random));
    CHECK(propertyAt(world, 8, 65, 8, "open") == "false");
    CHECK(propertyAt(world, 8, 64, 8, "open") == "false");

    // parity: `DoorBlock.neighborChanged` asks about the other half as well, so the lower half
    // opens from a signal it cannot see itself.
    REQUIRE(world.setBlockState(9, 65, 8, stateOf("redstone_block")));
    static_cast<void>(redstoneReactors::update(world, "world", 8, 63, 8, 1, random));
    CHECK(propertyAt(world, 8, 64, 8, "open") == "true");
    CHECK(propertyAt(world, 8, 65, 8, "open") == "true");
}

TEST_CASE("a lamp lights at once and waits four ticks to go dark") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("redstone_lamp", {{"lit", "false"}})));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    CHECK_FALSE(redstoneReactors::update(world, "world", 9, 64, 8, 1, random).empty());
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "true");

    // parity: it does not go out at once. It books four ticks and checks again then.
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    CHECK(redstoneReactors::update(world, "world", 9, 64, 8, 1, random).empty());
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "true");
    CHECK(world.hasScheduledBlockTick(8, 64, 8, Identifier::vanilla("redstone_lamp")));

    const std::optional<BlockChange> dark = redstoneReactors::lampTick(world, "world", 8, 64, 8);
    REQUIRE(dark.has_value());
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "false");

    // A lamp whose signal came back before the turn stays lit.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("redstone_lamp", {{"lit", "true"}})));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    CHECK_FALSE(redstoneReactors::lampTick(world, "world", 8, 64, 8).has_value());
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "true");
}

TEST_CASE("a copper bulb flips on the rising edge only") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("copper_bulb", {{"lit", "false"}, {"powered", "false"}})));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    static_cast<void>(redstoneReactors::update(world, "world", 9, 64, 8, 1, random));
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "true");

    // Going away leaves the light exactly as it is: that is the flip-flop.
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    static_cast<void>(redstoneReactors::update(world, "world", 9, 64, 8, 1, random));
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "true");
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "false");

    // The next signal flips it off again.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
    const std::vector<BlockChange> changes = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    CHECK(propertyAt(world, 8, 64, 8, "lit") == "false");
    REQUIRE_FALSE(changes.empty());
    CHECK(soundOf(changes.front()) == "block.copper_bulb.turn_off");
}

TEST_CASE("a note block plays on the way up and is silent on the way down") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    const std::vector<BlockChange> played = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    REQUIRE_FALSE(played.empty());
    CHECK_FALSE(played.front().effects.empty());
    CHECK(propertyAt(world, 8, 64, 8, "powered") == "true");

    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    const std::vector<BlockChange> quiet = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    REQUIRE_FALSE(quiet.empty());
    CHECK(quiet.front().effects.empty());
}

TEST_CASE("a bell rings the way it faces, and only when the signal arrives") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8,
                                stateOf("bell", {{"facing", "west"}, {"attachment", "floor"}, {"powered", "false"}})));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    const std::vector<BlockChange> rung = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    REQUIRE_FALSE(rung.empty());
    CHECK(soundOf(rung.front()) == "block.bell.use");
    // parity: the swing carries the way it faces as a three-dimensional direction value.
    const blockEffect::Event* swing = std::get_if<blockEffect::Event>(&rung.front().effects.front());
    REQUIRE(swing != nullptr);
    CHECK(swing->b0 == 1);
    CHECK(swing->b1 == 4);

    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    const std::vector<BlockChange> silent = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    REQUIRE_FALSE(silent.empty());
    CHECK(silent.front().effects.empty());
}

TEST_CASE("tnt lights and leaves an entity behind it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("tnt", {{"unstable", "false"}})));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    const std::vector<BlockChange> lit = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    REQUIRE_FALSE(lit.empty());
    CHECK(world.blockState(8, 64, 8) == registry::blocks::air().stateId);
    // The block is gone and the primed entity is queued for whoever spawns it.
    CHECK(world.takePrimedTnt().size() == 1);

    // parity: unstable tnt is the one a command placed to explode when broken; a signal leaves it.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("tnt", {{"unstable", "true"}})));
    static_cast<void>(redstoneReactors::update(world, "world", 9, 64, 8, 1, random));
    CHECK(world.blockState(8, 64, 8) != registry::blocks::air().stateId);
    CHECK(world.takePrimedTnt().empty());
}

TEST_CASE("the sound a door makes is the one its own code names") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(8, 64, 8,
                                stateOf("oak_trapdoor", {{"facing", "north"},
                                                         {"half", "bottom"},
                                                         {"open", "false"},
                                                         {"powered", "false"},
                                                         {"waterlogged", "false"}})));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    const std::vector<BlockChange> opened = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    REQUIRE_FALSE(opened.empty());
    // parity: `TrapDoorBlock.playSound`, the same sound a hand makes. It used to go through a
    // callback that production code never filled in, so a trapdoor on a signal swung in silence.
    CHECK(soundOf(opened.front()) == "minecraft:block.wooden_trapdoor.open");
    CHECK(propertyAt(world, 8, 64, 8, "open") == "true");
    // parity: a tenth of a tone of randomness under one, the same as a hand makes.
    const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&opened.front().effects.front());
    REQUIRE(sound != nullptr);
    CHECK(sound->pitch >= 0.9F);
    CHECK(sound->pitch < 1.0F);

    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::air().stateId));
    const std::vector<BlockChange> closed = redstoneReactors::update(world, "world", 9, 64, 8, 1, random);
    REQUIRE_FALSE(closed.empty());
    CHECK(soundOf(closed.front()) == "minecraft:block.wooden_trapdoor.close");
    CHECK(propertyAt(world, 8, 64, 8, "open") == "false");
}

TEST_CASE("a shelf joins its chain when the signal reaches it") {
    // parity: `ShelfBlock.neighborChanged`. `shelves::reactToSignal` was written, was right, and was
    // reached by nobody: the callback that led to it was never filled in outside a test.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::map<std::string, std::string> quiet{
        {"facing", "north"}, {"powered", "false"}, {"side_chain", "unconnected"}, {"waterlogged", "false"}};
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("oak_shelf", quiet)));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));

    rng::RandomSource random = rng::RandomSource::legacy(0);
    static_cast<void>(redstoneReactors::update(world, "world", 9, 64, 8, 1, random));

    CHECK(propertyAt(world, 8, 64, 8, "powered") == "true");
}

TEST_CASE("a sculk sensor hears a door swing, a gate swing and a note block, and nothing else") {
    // parity: `DoorBlock`, `TrapDoorBlock`, `FenceGateBlock` and `NoteBlock` all raise a game event
    // beside their sound, and none of them did here: a sensor next to a door on a clock heard
    // nothing at all. The three are worth one test because they share the rule that matters, which
    // is that the event goes out only when the thing really moves.
    const auto sleepingSensor = [] {
        return stateOf("sculk_sensor", {{"sculk_sensor_phase", "inactive"}, {"power", "0"}, {"waterlogged", "false"}});
    };
    const auto frequencyAt = [](const WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
        const Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
        REQUIRE(chunk != nullptr);
        const basalt::server::world::ChunkBlockEntity* entity = chunk->blockEntityAt(x & 15, y, z & 15);
        return entity == nullptr ? -1 : entity->data().getInt(std::string{vibrations::kFrequencyKey}, -1);
    };
    /// Runs the world's clock forward through the sensor's own booked turns.
    const auto settle = [](WorldState& world, std::int64_t& now, std::int32_t turns) {
        for (std::int32_t turn = 0; turn < turns; ++turn) {
            ++now;
            for (const std::array<std::int32_t, 3>& at : world.dueBlockTicks()) {
                static_cast<void>(vibrations::tick(world, at[0], at[1], at[2], now));
            }
        }
    };

    SUBCASE("a fence gate swinging open is a block_open") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(
            8, 64, 8,
            stateOf("oak_fence_gate",
                    {{"facing", "north"}, {"in_wall", "false"}, {"open", "false"}, {"powered", "false"}})));
        REQUIRE(world.setBlockState(8, 64, 10, sleepingSensor()));
        rng::RandomSource random = rng::RandomSource::legacy(0);
        std::int64_t now = 1;

        REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
        static_cast<void>(redstoneReactors::update(world, "world", 9, 64, 8, now, random));
        REQUIRE(propertyAt(world, 8, 64, 8, "open") == "true");
        settle(world, now, 4);

        CHECK(propertyAt(world, 8, 64, 10, "sculk_sensor_phase") == "active");
        CHECK(frequencyAt(world, 8, 64, 10) == 10);
    }

    SUBCASE("a note block that sounds is a note_block_play") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        REQUIRE(world.setBlockState(8, 64, 8, stateOf("note_block")));
        REQUIRE(world.setBlockState(8, 64, 10, sleepingSensor()));
        rng::RandomSource random = rng::RandomSource::legacy(0);
        std::int64_t now = 1;

        REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
        static_cast<void>(redstoneReactors::update(world, "world", 9, 64, 8, now, random));
        settle(world, now, 4);

        CHECK(frequencyAt(world, 8, 64, 10) == 10);
    }

    SUBCASE("a trapdoor that only changes powered says nothing") {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        // Already open by hand: the signal moves `powered` and leaves `open` where it was, so
        // vanilla makes no sound and raises no event.
        REQUIRE(world.setBlockState(8, 64, 8,
                                    stateOf("oak_trapdoor", {{"facing", "north"},
                                                             {"half", "bottom"},
                                                             {"open", "true"},
                                                             {"powered", "false"},
                                                             {"waterlogged", "false"}})));
        REQUIRE(world.setBlockState(8, 64, 10, sleepingSensor()));
        rng::RandomSource random = rng::RandomSource::legacy(0);
        std::int64_t now = 1;

        REQUIRE(world.setBlockState(9, 64, 8, stateOf("redstone_block")));
        static_cast<void>(redstoneReactors::update(world, "world", 9, 64, 8, now, random));
        REQUIRE(propertyAt(world, 8, 64, 8, "powered") == "true");
        settle(world, now, 6);

        CHECK(propertyAt(world, 8, 64, 10, "sculk_sensor_phase") == "inactive");
    }
}
