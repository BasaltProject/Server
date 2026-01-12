#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Pistons.hpp>

#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t piston(const std::string& facing, const std::string& extended,
                                  const std::string& path = "piston") {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), {{"facing", facing}, {"extended", extended}}, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();
    rng::RandomSource random = rng::RandomSource::legacy(1);

    Loaded() { state.getOrGenerateChunk(0, 0); }

    [[nodiscard]] pistons::Context context() {
        return pistons::Context{.world = state, .worldName = "overworld", .minY = -64, .maxY = 319, .random = random};
    }
};

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

} // namespace

TEST_CASE("air moves, and so does anything ordinary") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));

    CHECK(pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
    // Air is always pushable, which is what ends a line.
    CHECK(pistons::isPushable(loaded.context(), 9, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
}

TEST_CASE("obsidian and its family never move") {
    Loaded loaded;
    WorldState& world = loaded.state;
    for (const std::string& path : {"obsidian", "crying_obsidian", "respawn_anchor", "reinforced_deepslate"}) {
        REQUIRE(world.setBlockState(8, 64, 8, stateOf(path)));
        CHECK_FALSE(pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, false,
                                        registry::BlockFace::East));
    }
}

TEST_CASE("what cannot be broken cannot be pushed") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("bedrock")));

    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
}

TEST_CASE("a block that breaks only moves when the push is allowed to break it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("torch")));

    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
    CHECK(pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, true, registry::BlockFace::East));
}

TEST_CASE("a block with contents stays where it is") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // parity: a chest is NORMAL and still refuses, because a piston does not carry block entities.
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("chest")));

    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
}

TEST_CASE("nothing is pushed out of the world") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, -64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 319, 8, stateOf("stone")));

    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, -64, 8, registry::BlockFace::Down, false, registry::BlockFace::Down));
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, 319, 8, registry::BlockFace::Up, false, registry::BlockFace::Up));
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, -65, 8, registry::BlockFace::East, false, registry::BlockFace::East));
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, 320, 8, registry::BlockFace::East, false, registry::BlockFace::East));
    // Sideways is fine at either end.
    CHECK(
        pistons::isPushable(loaded.context(), 8, -64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
}

TEST_CASE("an extended piston is holding its own head and will not move") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "true")));
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));

    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    CHECK(pistons::isPushable(loaded.context(), 8, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
}

TEST_CASE("a push is the line in front of the head, in the order it has to be written") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    for (std::int32_t x = 9; x <= 11; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, stateOf("stone")));
    }

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, true);

    REQUIRE(push.has_value());
    REQUIRE(push->moved.size() == 3);
    CHECK(push->moved[0] == pistons::Position{9, 64, 8});
    CHECK(push->moved[2] == pistons::Position{11, 64, 8});
    CHECK(push->destroyed.empty());
}

TEST_CASE("a line of thirteen refuses to move at all") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(0, 64, 8, piston("east", "false")));
    for (std::int32_t x = 1; x <= 12; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, stateOf("stone")));
    }
    CHECK(pistons::resolve(loaded.context(), 0, 64, 8, registry::BlockFace::East, true).has_value());

    // parity: `PistonStructureResolver`'s own twelve, so the thirteenth block stops the push.
    REQUIRE(world.setBlockState(13, 64, 8, stateOf("stone")));
    CHECK_FALSE(pistons::resolve(loaded.context(), 0, 64, 8, registry::BlockFace::East, true).has_value());
}

TEST_CASE("something that breaks is broken rather than moved") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("torch")));

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, true);

    REQUIRE(push.has_value());
    CHECK(push->moved.size() == 1);
    REQUIRE(push->destroyed.size() == 1);
    CHECK(push->destroyed[0] == pistons::Position{10, 64, 8});
}

TEST_CASE("a torch right in front of the head is simply broken") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("torch")));

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, true);

    REQUIRE(push.has_value());
    CHECK(push->moved.empty());
    REQUIRE(push->destroyed.size() == 1);
    CHECK(push->destroyed[0] == pistons::Position{9, 64, 8});
    // Pulling back looks two blocks out instead, where there is nothing at all, and breaks nothing.
    const std::optional<pistons::Push> back =
        pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, false);
    REQUIRE(back.has_value());
    CHECK(back->moved.empty());
    CHECK(back->destroyed.empty());
}

TEST_CASE("obsidian in the line stops the push dead") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("obsidian")));

    CHECK_FALSE(pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, true).has_value());
}

TEST_CASE("a slime block drags what is stuck to its sides") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("slime_block")));
    // One above and one below, off the push axis, which is what a branch is.
    REQUIRE(world.setBlockState(9, 65, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(9, 63, 8, stateOf("stone")));

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, true);

    REQUIRE(push.has_value());
    CHECK(push->moved.size() == 3);
}

TEST_CASE("slime and honey stick to anything but each other") {
    const auto dragsAlong = [](const std::string& first, const std::string& second) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
        REQUIRE(world.setBlockState(9, 64, 8, stateOf(first)));
        REQUIRE(world.setBlockState(9, 65, 8, stateOf(second)));
        const std::optional<pistons::Push> push =
            pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, true);
        return push.has_value() && push->moved.size() == 2;
    };

    CHECK(dragsAlong("slime_block", "stone"));
    CHECK(dragsAlong("honey_block", "stone"));
    // parity: `canStickToEachOther`, the one pair that does not stick.
    CHECK_FALSE(dragsAlong("slime_block", "honey_block"));
    CHECK_FALSE(dragsAlong("honey_block", "slime_block"));
    CHECK_FALSE(dragsAlong("stone", "stone"));
}

TEST_CASE("carrying the push out moves every block one along, back to front") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("dirt")));
    const pistons::Push push{.moved = {{9, 64, 8}, {10, 64, 8}}, .destroyed = {}};

    const std::vector<BlockChange> changes = pistons::apply(loaded.context(), push, registry::BlockFace::East);

    CHECK_FALSE(changes.empty());
    CHECK(world.blockState(9, 64, 8) == air());
    CHECK(world.blockState(10, 64, 8) == stateOf("stone"));
    CHECK(world.blockState(11, 64, 8) == stateOf("dirt"));
}

TEST_CASE("what the push broke is gone before anything moves") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("torch")));
    const pistons::Push push{.moved = {{9, 64, 8}}, .destroyed = {{10, 64, 8}}};

    const std::vector<BlockChange> changes = pistons::apply(loaded.context(), push, registry::BlockFace::East);

    // The break comes first, at its own coordinates, and only then does the stone land on top of it.
    REQUIRE(changes.size() == 3);
    CHECK(changes[0].stateId == air());
    CHECK(changes[0].x == 10);
    CHECK(changes[0].y == 64);
    CHECK(changes[0].z == 8);
    CHECK(changes[1].x == 10);
    CHECK(changes[1].y == 64);
    CHECK(changes[1].z == 8);
    CHECK(changes[1].stateId == stateOf("stone"));
    CHECK(changes[2].x == 9);
    CHECK(changes[2].y == 64);
    CHECK(changes[2].z == 8);
    CHECK(changes[2].stateId == air());
    CHECK(world.blockState(10, 64, 8) == stateOf("stone"));
}

TEST_CASE("every coordinate of a carried block is written where it belongs") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // Distinct on all three axes, so reading the wrong one is a different block.
    REQUIRE(world.setBlockState(9, 64, 5, stateOf("stone")));
    const pistons::Push push{.moved = {{9, 64, 5}}, .destroyed = {{2, 70, 3}}};
    REQUIRE(world.setBlockState(2, 70, 3, stateOf("dirt")));

    const std::vector<BlockChange> changes = pistons::apply(loaded.context(), push, registry::BlockFace::Up);

    REQUIRE(changes.size() == 3);
    CHECK(changes[0].x == 2);
    CHECK(changes[0].y == 70);
    CHECK(changes[0].z == 3);
    CHECK(changes[1].x == 9);
    CHECK(changes[1].y == 65);
    CHECK(changes[1].z == 5);
    CHECK(changes[1].stateId == stateOf("stone"));
    CHECK(changes[2].x == 9);
    CHECK(changes[2].y == 64);
    CHECK(changes[2].z == 5);
    CHECK(world.blockState(2, 70, 3) == air());
    CHECK(world.blockState(9, 65, 5) == stateOf("stone"));
    CHECK(world.blockState(9, 64, 5) == air());
}

TEST_CASE("a block outside the world is not carried anywhere") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    const pistons::Push push{.moved = {{300, 64, 300}, {9, 64, 8}}, .destroyed = {{300, 64, 300}}};

    const std::vector<BlockChange> changes = pistons::apply(loaded.context(), push, registry::BlockFace::East);

    // Only the one block that exists moves; the unloaded chunk yields nothing at all.
    REQUIRE(changes.size() == 2);
    CHECK(changes[0].x == 10);
    CHECK(changes[1].x == 9);
}

TEST_CASE("a powered piston books its move and says so with a sound") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;

    pistons::trigger(loaded.context(), 8, 64, 8, effects);

    REQUIRE(effects.size() == 1);
    REQUIRE(effects[0].effects.size() == 2);
    const blockEffect::Event* event = std::get_if<blockEffect::Event>(&effects[0].effects[0]);
    REQUIRE(event != nullptr);
    // parity: 0 to push out, and the face it moves along as the second byte.
    CHECK(event->b0 == 0);
    CHECK(event->b1 == static_cast<std::int32_t>(registry::BlockFace::East));
    const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&effects[0].effects[1]);
    REQUIRE(sound != nullptr);
    CHECK(sound->name == "block.piston.extend");
    CHECK(sound->volume == doctest::Approx(0.5F));
    CHECK(sound->pitch >= 0.6F);
    CHECK(sound->pitch < 0.85F);

    // parity: the move lands two ticks later, and every call to dueBlockTicks is one tick.
    CHECK(world.dueBlockTicks().empty());
    const std::vector<std::array<std::int32_t, 3>> due = world.dueBlockTicks();
    REQUIRE(due.size() == 1);
    CHECK(due[0] == std::array<std::int32_t, 3>{8, 64, 8});
}

TEST_CASE("a piston cannot be powered by what it is about to push") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    // parity: `getNeighborSignal` skips the piston's own face.
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;

    pistons::trigger(loaded.context(), 8, 64, 8, effects);

    CHECK(effects.empty());
}

TEST_CASE("a signal two blocks up fires the piston too") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    // parity: the quirk where the block above is asked for itself and for what is over it.
    REQUIRE(world.setBlockState(8, 66, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;

    pistons::trigger(loaded.context(), 8, 64, 8, effects);

    CHECK(effects.size() == 1);
}

TEST_CASE("the move landing puts the head down and the blocks one along") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;
    pistons::trigger(loaded.context(), 8, 64, 8, effects);
    // Mid-slide the block is a moving_piston carrying what it will become.
    CHECK(registry::blockStateDefinitions::blockOf(*world.blockState(9, 64, 8))->path() == "moving_piston");

    const std::vector<BlockChange> changes = pistons::finish(loaded.context(), 8, 64, 8);

    // parity: medido contra el jar. El cuerpo lee extended y el destino ya tiene un moving_piston
    // desde que empieza el deslizamiento, asi que el aterrizaje solo convierte cada uno en lo que
    // lleva, donde ya esta.
    REQUIRE(changes.size() == 2);
    CHECK(changes[0] == BlockChange{.x = 10, .y = 64, .z = 8, .stateId = stateOf("stone")});
    CHECK(changes[1].x == 9);
    CHECK(changes[1].y == 64);
    CHECK(changes[1].z == 8);
    CHECK(registry::blockStateDefinitions::blockOf(changes[1].stateId)->path() == "piston_head");
    CHECK(world.blockState(10, 64, 8) == stateOf("stone"));
    CHECK(world.blockState(9, 64, 8) == changes[1].stateId);
    CHECK(propertyOf(*world.blockState(8, 64, 8), "extended") == "true");
}

TEST_CASE("a piston pointing up lands its blocks from the top down") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("up", "false")));
    REQUIRE(world.setBlockState(8, 65, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 66, 8, stateOf("dirt")));
    REQUIRE(world.setBlockState(9, 64, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;
    pistons::trigger(loaded.context(), 8, 64, 8, effects);

    const std::vector<BlockChange> changes = pistons::finish(loaded.context(), 8, 64, 8);

    // parity: furthest along the push first, which up an axis is the highest block.
    REQUIRE(changes.size() == 3);
    CHECK(changes[0] == BlockChange{.x = 8, .y = 67, .z = 8, .stateId = stateOf("dirt")});
    CHECK(changes[1] == BlockChange{.x = 8, .y = 66, .z = 8, .stateId = stateOf("stone")});
    CHECK(changes[2].y == 65);
    CHECK(propertyOf(changes[2].stateId, "facing") == "up");
    CHECK(world.blockState(8, 67, 8) == stateOf("dirt"));
    CHECK(world.blockState(8, 66, 8) == stateOf("stone"));
}

namespace {

/// Puts a leftover `moving_piston` block entity at a world position, as somebody else's push would.
void stale(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z) {
    Chunk* chunk = world.chunkIfLoaded(x >> 4, z >> 4);
    REQUIRE(chunk != nullptr);
    basalt::wire::nbt::NbtCompound data;
    data.putInt("blockState", stateOf("dirt"));
    data.putString("facing", "east");
    data.putBoolean("extending", true);
    data.putBoolean("source", false);
    chunk->setBlockEntity(
        basalt::server::world::ChunkBlockEntity{x & 15, y, z & 15, pistons::block(), std::move(data)});
}

} // namespace

TEST_CASE("a block mid-slide is carried up to thirteen away and no further") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 0);
    world.getOrGenerateChunk(0, -1);
    world.getOrGenerateChunk(-1, -1);
    world.getOrGenerateChunk(1, 1);
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::named("redstone_block")->stateId));

    // parity: the twelve blocks a push reaches, plus the one it lands on, measured on each axis.
    stale(world, 8, 77, 8);
    stale(world, 8, 78, 8);
    stale(world, 21, 64, 8);
    stale(world, 22, 64, 8);
    stale(world, 8, 64, -5);
    stale(world, 8, 64, -6);
    // And the corners of the nine chunks the scan covers, which are well within reach.
    stale(world, -1, 64, -1);
    stale(world, 16, 64, 16);

    static_cast<void>(pistons::finish(loaded.context(), 8, 64, 8));

    // Cada uno aterriza donde ya estaba su moving_piston, que es lo que hace `finalTick`. Lo que
    // mide este caso es el alcance del barrido, no el desplazamiento.
    CHECK(world.blockState(8, 77, 8) == stateOf("dirt"));
    CHECK(world.blockState(8, 78, 8) == air());
    CHECK(world.blockState(21, 64, 8) == stateOf("dirt"));
    CHECK(world.blockState(22, 64, 8) == air());
    CHECK(world.blockState(8, 64, -5) == stateOf("dirt"));
    CHECK(world.blockState(8, 64, -6) == air());
    CHECK(world.blockState(-1, 64, -1) == stateOf("dirt"));
    CHECK(world.blockState(16, 64, 16) == stateOf("dirt"));
}

TEST_CASE("a sticky piston brings one block back with it and a plain one does not") {
    const auto pulled = [](const std::string& path) -> std::vector<BlockChange> {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, piston("east", "true", path)));
        const std::optional<std::int32_t> head = registry::blockStateDefinitions::stateIdOf(
            basalt::wire::common::Identifier::vanilla("piston_head"),
            {{"facing", "east"}, {"type", path == "sticky_piston" ? "sticky" : "normal"}, {"short", "false"}},
            std::nullopt);
        REQUIRE(head.has_value());
        REQUIRE(world.setBlockState(9, 64, 8, *head));
        REQUIRE(world.setBlockState(10, 64, 8, stateOf("stone")));

        return pistons::finish(loaded.context(), 8, 64, 8);
    };

    // parity: the arm goes, the block takes its place, and the body loses its own `extended`.
    const std::vector<BlockChange> sticky = pulled("sticky_piston");
    REQUIRE(sticky.size() == 4);
    CHECK(sticky[0] == BlockChange{.x = 9, .y = 64, .z = 8, .stateId = air()});
    CHECK(sticky[1] == BlockChange{.x = 9, .y = 64, .z = 8, .stateId = stateOf("stone")});
    CHECK(sticky[2] == BlockChange{.x = 10, .y = 64, .z = 8, .stateId = air()});
    CHECK(sticky[3].x == 8);
    CHECK(sticky[3].y == 64);
    CHECK(sticky[3].z == 8);
    CHECK(propertyOf(sticky[3].stateId, "extended") == "false");

    const std::vector<BlockChange> plain = pulled("piston");
    REQUIRE(plain.size() == 2);
    CHECK(plain[0] == BlockChange{.x = 9, .y = 64, .z = 8, .stateId = air()});
    CHECK(plain[1].x == 8);
    CHECK(propertyOf(plain[1].stateId, "extended") == "false");
}

TEST_CASE("the blocks a piston moves shove what they sweep over") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const DroppedItem& dropped = world.spawnItem(DroppedItem{.entityId = 5,
                                                             .uuid = basalt::wire::common::Uuid::random(),
                                                             .item = basalt::wire::common::Identifier::vanilla("stone"),
                                                             .count = 1,
                                                             .x = 9.5,
                                                             .y = 64.2,
                                                             .z = 8.5});
    CHECK(dropped.entityId == 5);
    std::vector<pistons::PlayerPosition> players{{.uuid = "someone", .x = 9.5, .y = 64.9, .z = 8.5}};
    std::optional<pistons::PlayerPosition> shoved;

    pistons::dragAlong(loaded.context(), {{9, 64, 8}}, registry::BlockFace::East, players,
                       [&](const pistons::PlayerPosition& moved) { shoved = moved; });

    CHECK(world.items()[0].x == doctest::Approx(10.5));
    REQUIRE(shoved.has_value());
    CHECK(shoved->x == doctest::Approx(10.5));
    CHECK(shoved->uuid == "someone");
}

TEST_CASE("what the blocks do not sweep is left alone") {
    Loaded loaded;
    WorldState& world = loaded.state;
    static_cast<void>(world.spawnItem(DroppedItem{.entityId = 5,
                                                  .uuid = basalt::wire::common::Uuid::random(),
                                                  .item = basalt::wire::common::Identifier::vanilla("stone"),
                                                  .count = 1,
                                                  .x = 20.5,
                                                  .y = 64.2,
                                                  .z = 8.5}));

    pistons::dragAlong(loaded.context(), {{9, 64, 8}}, registry::BlockFace::East);

    CHECK(world.items()[0].x == doctest::Approx(20.5));
}

TEST_CASE("anything that is not a piston is left alone") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("stone")));
    std::vector<BlockChange> effects;

    pistons::trigger(loaded.context(), 8, 64, 8, effects);
    CHECK(effects.empty());
    CHECK(pistons::finish(loaded.context(), 8, 64, 8).empty());
    CHECK(pistons::finish(loaded.context(), 300, 64, 300).empty());
}

TEST_CASE("nothing is pushed past the roof or the floor") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 319, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(8, -64, 8, stateOf("stone")));

    // parity: `isPushable` refuses the last layer in the direction of travel, and only that one.
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, 319, 8, registry::BlockFace::Up, false, registry::BlockFace::Up));
    CHECK(pistons::isPushable(loaded.context(), 8, 319, 8, registry::BlockFace::North, false,
                              registry::BlockFace::North));
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 8, -64, 8, registry::BlockFace::Down, false, registry::BlockFace::Down));
    CHECK(pistons::isPushable(loaded.context(), 8, -64, 8, registry::BlockFace::North, false,
                              registry::BlockFace::North));
}

TEST_CASE("glazed terracotta only moves when the push comes at it head on") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("white_glazed_terracotta")));

    // parity: the only `PUSH_ONLY` block there is, and what it reads is the direction it was reached
    // from, not its own face.
    CHECK(pistons::isPushable(loaded.context(), 9, 64, 8, registry::BlockFace::East, false, registry::BlockFace::East));
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 9, 64, 8, registry::BlockFace::East, false, registry::BlockFace::West));
    CHECK_FALSE(
        pistons::isPushable(loaded.context(), 9, 64, 8, registry::BlockFace::East, false, registry::BlockFace::Up));
}

TEST_CASE("a slime block never drags glazed terracotta along with it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("slime_block")));
    REQUIRE(world.setBlockState(9, 65, 8, stateOf("white_glazed_terracotta")));
    REQUIRE(world.setBlockState(9, 63, 8, stateOf("stone")));

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, true);

    // parity: a branch is reached from a face across the push axis, which is never the push itself.
    REQUIRE(push.has_value());
    CHECK(push->moved == std::vector<pistons::Position>{{9, 64, 8}, {9, 63, 8}});
}

TEST_CASE("a sticky piston cannot pull glazed terracotta back") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "true", "sticky_piston")));
    const std::optional<std::int32_t> head = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla("piston_head"),
        {{"facing", "east"}, {"type", "sticky"}, {"short", "false"}}, std::nullopt);
    REQUIRE(head.has_value());
    REQUIRE(world.setBlockState(9, 64, 8, *head));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("white_glazed_terracotta")));

    static_cast<void>(pistons::finish(loaded.context(), 8, 64, 8));

    CHECK(world.blockState(9, 64, 8) == air());
    CHECK(world.blockState(10, 64, 8) == stateOf("white_glazed_terracotta"));
}

TEST_CASE("pulling back starts two blocks out, past the arm") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "true", "sticky_piston")));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("stone")));

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 8, 64, 8, registry::BlockFace::East, false);

    // parity: the block the arm is holding, dragged the other way.
    REQUIRE(push.has_value());
    CHECK(push->moved == std::vector<pistons::Position>{{10, 64, 8}});
}

TEST_CASE("a line of exactly twelve still moves") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(0, 64, 8, piston("east", "false")));
    for (std::int32_t x = 1; x <= 12; ++x) {
        REQUIRE(world.setBlockState(x, 64, 8, stateOf("stone")));
    }

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 0, 64, 8, registry::BlockFace::East, true);

    REQUIRE(push.has_value());
    CHECK(push->moved.size() == 12);
    CHECK(push->moved.front() == pistons::Position{1, 64, 8});
    CHECK(push->moved.back() == pistons::Position{12, 64, 8});
}

TEST_CASE("a sticky piston with nothing to pull just takes its head back") {
    const auto pulledState = [](const std::string& facing) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, piston("east", "true", "sticky_piston")));
        const std::optional<std::int32_t> head = registry::blockStateDefinitions::stateIdOf(
            basalt::wire::common::Identifier::vanilla("piston_head"),
            {{"facing", "east"}, {"type", "sticky"}, {"short", "false"}}, std::nullopt);
        REQUIRE(head.has_value());
        REQUIRE(world.setBlockState(9, 64, 8, *head));
        REQUIRE(world.setBlockState(10, 64, 8, stateOf(facing)));

        static_cast<void>(pistons::finish(loaded.context(), 8, 64, 8));
        return world.blockState(9, 64, 8);
    };

    // Obsidian never comes back with it, and neither does a chest, which holds its own contents.
    CHECK(pulledState("obsidian") == air());
    CHECK(pulledState("chest") == air());
    CHECK(pulledState("dirt") == stateOf("dirt"));
}

TEST_CASE("the blocks mid-slide land furthest along first") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(10, 64, 8, stateOf("dirt")));
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;
    pistons::trigger(loaded.context(), 8, 64, 8, effects);

    const std::vector<BlockChange> changes = pistons::finish(loaded.context(), 8, 64, 8);

    // La tierra del extremo se escribe antes que la piedra que la sigue, para que nada se pise.
    REQUIRE(changes.size() >= 2);
    CHECK(changes[0].x == 11);
    CHECK(changes[0].stateId == stateOf("dirt"));
    CHECK(changes[1].x == 10);
    CHECK(changes[1].stateId == stateOf("stone"));
    CHECK(world.blockState(11, 64, 8) == stateOf("dirt"));
    CHECK(world.blockState(10, 64, 8) == stateOf("stone"));
}

TEST_CASE("a block mid-slide carries the state it is about to become") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false", "sticky_piston")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("dirt")));
    REQUIRE(world.setBlockState(8, 65, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;

    pistons::trigger(loaded.context(), 8, 64, 8, effects);

    const Chunk* chunk = world.chunkIfLoaded(0, 0);
    REQUIRE(chunk != nullptr);
    // parity: `moveBlocks` deja dos, y cada uno en su destino: la tierra en la casilla a la que va,
    // y el brazo en la de delante del piston llevando la cabeza que sera.
    const std::vector<basalt::server::world::ChunkBlockEntity> entities = chunk->blockEntities();
    REQUIRE(entities.size() == 2);
    const auto at = [&entities](std::int32_t localX) -> const basalt::server::world::ChunkBlockEntity& {
        const auto found = std::ranges::find_if(
            entities, [localX](const basalt::server::world::ChunkBlockEntity& one) { return one.localX() == localX; });
        REQUIRE(found != entities.end());
        return *found;
    };
    const basalt::server::world::ChunkBlockEntity& carried = at(10);
    CHECK(carried.y() == 64);
    CHECK(carried.localZ() == 8);
    CHECK(carried.data().getInt("blockState") == stateOf("dirt"));
    CHECK(carried.data().getString("facing") == "east");
    CHECK(carried.data().getBoolean("extending"));
    CHECK_FALSE(carried.data().getBoolean("source"));
    const basalt::server::world::ChunkBlockEntity& arm = at(9);
    CHECK(registry::blockStateDefinitions::blockOf(arm.data().getInt("blockState"))->path() == "piston_head");
    CHECK(arm.data().getBoolean("source"));
    CHECK(propertyOf(*world.blockState(9, 64, 8), "type") == "sticky");
}

TEST_CASE("what a block sweeps is the square it leaves and the one it lands on") {
    Loaded loaded;
    WorldState& world = loaded.state;
    std::vector<pistons::PlayerPosition> players;
    // One player on each face of the box a block at (9, 64, 8) sweeps going east: x in [9, 11),
    // y in [64, 65), z in [8, 9).
    const std::array<std::array<double, 3>, 8> spots{{{9.0, 64.0, 8.0},
                                                      {10.99, 64.99, 8.99},
                                                      {8.99, 64.5, 8.5},
                                                      {11.0, 64.5, 8.5},
                                                      {9.5, 63.99, 8.5},
                                                      {9.5, 65.0, 8.5},
                                                      {9.5, 64.5, 7.99},
                                                      {9.5, 64.5, 9.0}}};
    for (std::size_t index = 0; index < spots.size(); ++index) {
        players.push_back(pistons::PlayerPosition{
            .uuid = std::to_string(index), .x = spots[index][0], .y = spots[index][1], .z = spots[index][2]});
    }
    std::vector<std::string> shoved;

    pistons::dragAlong(loaded.context(), {{9, 64, 8}}, registry::BlockFace::East, players,
                       [&](const pistons::PlayerPosition& moved) { shoved.push_back(moved.uuid); });

    CHECK(shoved == std::vector<std::string>{"0", "1"});
    CHECK(world.items().empty());
}

TEST_CASE("a block sweeping downwards covers the square below it") {
    Loaded loaded;
    std::vector<pistons::PlayerPosition> players{{.uuid = "below", .x = 9.5, .y = 63.5, .z = 8.5},
                                                 {.uuid = "above", .x = 9.5, .y = 65.5, .z = 8.5},
                                                 {.uuid = "inside", .x = 9.5, .y = 64.5, .z = 8.5}};
    std::vector<std::string> shoved;

    pistons::dragAlong(loaded.context(), {{9, 64, 8}}, registry::BlockFace::Down, players,
                       [&](const pistons::PlayerPosition& moved) { shoved.push_back(moved.uuid); });

    CHECK(shoved == std::vector<std::string>{"below", "inside"});
}

TEST_CASE("a block sweeping north covers the square in front of it") {
    Loaded loaded;
    std::vector<pistons::PlayerPosition> players{{.uuid = "ahead", .x = 9.5, .y = 64.5, .z = 7.5},
                                                 {.uuid = "behind", .x = 9.5, .y = 64.5, .z = 9.5},
                                                 {.uuid = "inside", .x = 9.5, .y = 64.5, .z = 8.5}};
    std::vector<std::string> shoved;

    pistons::dragAlong(loaded.context(), {{9, 64, 8}}, registry::BlockFace::North, players,
                       [&](const pistons::PlayerPosition& moved) { shoved.push_back(moved.uuid); });

    CHECK(shoved == std::vector<std::string>{"ahead", "inside"});
}

TEST_CASE("a piston is fired from any side but the one it faces") {
    const auto fires = [](std::int32_t x, std::int32_t y, std::int32_t z) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(8, 64, 8, piston("east", "false")));
        REQUIRE(world.setBlockState(x, y, z, registry::blocks::named("redstone_block")->stateId));
        std::vector<BlockChange> effects;
        pistons::trigger(loaded.context(), 8, 64, 8, effects);
        return !effects.empty();
    };

    CHECK(fires(7, 64, 8));
    CHECK(fires(8, 63, 8));
    CHECK(fires(8, 64, 7));
    CHECK(fires(8, 64, 9));
    // parity: the ring one higher is asked too, which is the quasi-connectivity everyone builds with.
    CHECK(fires(7, 65, 8));
    CHECK(fires(9, 65, 8));
    CHECK(fires(8, 66, 8));
    // But not the block under that ring, which nothing asks.
    CHECK_FALSE(fires(8, 62, 8));
    CHECK_FALSE(fires(7, 66, 8));
}

TEST_CASE("a branch drags the sticky line behind it as well") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(0, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(1, 64, 8, stateOf("slime_block")));
    REQUIRE(world.setBlockState(1, 65, 8, stateOf("slime_block")));
    // parity: `addBlockLine` walks back along the push before it walks forward, so the block west of
    // the branch comes along even though nothing is pushing it directly.
    REQUIRE(world.setBlockState(0, 65, 8, stateOf("slime_block")));

    const std::optional<pistons::Push> push =
        pistons::resolve(loaded.context(), 0, 64, 8, registry::BlockFace::East, true);

    REQUIRE(push.has_value());
    CHECK(push->moved == std::vector<pistons::Position>{{1, 64, 8}, {0, 65, 8}, {1, 65, 8}});
}

TEST_CASE("the chain behind a branch stops where it stops sticking") {
    const auto movedWith = [](const std::string& behind) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(0, 64, 8, piston("east", "false")));
        REQUIRE(world.setBlockState(1, 64, 8, stateOf("slime_block")));
        REQUIRE(world.setBlockState(1, 65, 8, stateOf("slime_block")));
        REQUIRE(world.setBlockState(0, 65, 8, stateOf(behind)));
        const std::optional<pistons::Push> push =
            pistons::resolve(loaded.context(), 0, 64, 8, registry::BlockFace::East, true);
        REQUIRE(push.has_value());
        return push->moved;
    };

    // parity: one sticky block on either side is enough, so a plain block does come along.
    CHECK(movedWith("stone") == std::vector<pistons::Position>{{1, 64, 8}, {0, 65, 8}, {1, 65, 8}});
    // Honey and slime are the one pair that refuses each other, so the chain ends there.
    CHECK(movedWith("honey_block") == std::vector<pistons::Position>{{1, 64, 8}, {1, 65, 8}});
    // And obsidian is not going anywhere, which ends the chain rather than the push.
    CHECK(movedWith("obsidian") == std::vector<pistons::Position>{{1, 64, 8}, {1, 65, 8}});
}

TEST_CASE("a branch that would take the push past twelve refuses the whole thing") {
    const auto resolves = [](std::int32_t line, bool branch) {
        Loaded loaded;
        WorldState& world = loaded.state;
        REQUIRE(world.setBlockState(0, 64, 8, piston("east", "false")));
        for (std::int32_t x = 1; x <= line; ++x) {
            REQUIRE(world.setBlockState(x, 64, 8, stateOf("slime_block")));
        }
        if (branch) {
            REQUIRE(world.setBlockState(1, 65, 8, stateOf("slime_block")));
        }
        return pistons::resolve(loaded.context(), 0, 64, 8, registry::BlockFace::East, true).has_value();
    };

    CHECK(resolves(12, false));
    CHECK(resolves(11, true));
    CHECK_FALSE(resolves(12, true));
    CHECK_FALSE(resolves(13, false));
}

TEST_CASE("a piston pulling back says so with its own event and sound") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 64, 8, piston("east", "true")));
    std::vector<BlockChange> effects;

    pistons::trigger(loaded.context(), 8, 64, 8, effects);

    REQUIRE(effects.size() == 1);
    REQUIRE(effects[0].effects.size() == 2);
    const blockEffect::Event* event = std::get_if<blockEffect::Event>(&effects[0].effects[0]);
    REQUIRE(event != nullptr);
    // parity: 1 to pull back, against the 0 that pushes out.
    CHECK(event->b0 == 1);
    CHECK(event->b1 == static_cast<std::int32_t>(registry::BlockFace::East));
    const blockEffect::Sound* sound = std::get_if<blockEffect::Sound>(&effects[0].effects[1]);
    REQUIRE(sound != nullptr);
    CHECK(sound->name == "block.piston.contract");
    CHECK(sound->pitch >= 0.6F);
    CHECK(sound->pitch < 0.75F);
}

TEST_CASE("a push that crosses into the next chunk still finds its blocks") {
    Loaded loaded;
    WorldState& world = loaded.state;
    world.getOrGenerateChunk(1, 0);
    REQUIRE(world.setBlockState(14, 64, 8, piston("east", "false")));
    REQUIRE(world.setBlockState(15, 64, 8, stateOf("stone")));
    REQUIRE(world.setBlockState(16, 64, 8, stateOf("dirt")));
    REQUIRE(world.setBlockState(14, 65, 8, registry::blocks::named("redstone_block")->stateId));
    std::vector<BlockChange> effects;
    pistons::trigger(loaded.context(), 14, 64, 8, effects);

    // Los dos destinos caen en el chunk siguiente, cada uno en sus coordenadas locales: la piedra
    // que sale de 15 aterriza en 16 y la tierra que sale de 16 en 17.
    const Chunk* next = world.chunkIfLoaded(1, 0);
    REQUIRE(next != nullptr);
    const std::vector<basalt::server::world::ChunkBlockEntity> entities = next->blockEntities();
    REQUIRE(entities.size() == 2);
    const auto carried = [&entities](std::int32_t localX) {
        const auto found = std::ranges::find_if(
            entities, [localX](const basalt::server::world::ChunkBlockEntity& one) { return one.localX() == localX; });
        REQUIRE(found != entities.end());
        return found->data().getInt("blockState");
    };
    CHECK(carried(0) == stateOf("stone"));
    CHECK(carried(1) == stateOf("dirt"));

    static_cast<void>(pistons::finish(loaded.context(), 14, 64, 8));

    CHECK(world.blockState(17, 64, 8) == stateOf("dirt"));
    CHECK(world.blockState(16, 64, 8) == stateOf("stone"));
    CHECK(registry::blockStateDefinitions::blockOf(*world.blockState(15, 64, 8))->path() == "piston_head");
}

TEST_CASE("a block sweeping west and one sweeping up cover the square they land on") {
    Loaded loaded;
    std::vector<pistons::PlayerPosition> west{{.uuid = "ahead", .x = 8.5, .y = 64.5, .z = 8.5},
                                              {.uuid = "behind", .x = 10.5, .y = 64.5, .z = 8.5},
                                              {.uuid = "inside", .x = 9.5, .y = 64.5, .z = 8.5}};
    std::vector<std::string> shovedWest;
    pistons::dragAlong(loaded.context(), {{9, 64, 8}}, registry::BlockFace::West, west,
                       [&](const pistons::PlayerPosition& moved) { shovedWest.push_back(moved.uuid); });
    CHECK(shovedWest == std::vector<std::string>{"ahead", "inside"});

    std::vector<pistons::PlayerPosition> up{{.uuid = "over", .x = 9.5, .y = 65.5, .z = 8.5},
                                            {.uuid = "under", .x = 9.5, .y = 63.5, .z = 8.5},
                                            {.uuid = "inside", .x = 9.5, .y = 64.5, .z = 8.5}};
    std::vector<std::string> shovedUp;
    pistons::dragAlong(loaded.context(), {{9, 64, 8}}, registry::BlockFace::Up, up,
                       [&](const pistons::PlayerPosition& moved) { shovedUp.push_back(moved.uuid); });
    CHECK(shovedUp == std::vector<std::string>{"over", "inside"});
}
