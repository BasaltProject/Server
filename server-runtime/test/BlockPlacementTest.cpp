#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/BlockPlacement.hpp>

#include <doctest.h>

#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::protocol::play::Direction;
namespace registry = basalt::server::registry;
namespace rng = basalt::server::random;

namespace {

[[nodiscard]] basalt::wire::common::Identifier block(const std::string& path) {
    return basalt::wire::common::Identifier::vanilla(path);
}

[[nodiscard]] std::int32_t defaultOf(const std::string& path) {
    const registry::BlockState* found = registry::blocks::named(path);
    REQUIRE(found != nullptr);
    return found->stateId;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

/// The state a block goes down as, with the placement details that matter spelled out.
[[nodiscard]] std::int32_t placed(const std::string& path, const blockPlacement::Placement& placement) {
    rng::RandomSource random = rng::RandomSource::legacy(1);
    return blockPlacement::stateFor(block(path), defaultOf(path), placement, random);
}

} // namespace

TEST_CASE("the quarter of a yaw a player is looking along") {
    // parity: `Direction.fromYRot`, south at zero and turning clockwise.
    CHECK(blockPlacement::playerDirection(0.0F) == Direction::South);
    CHECK(blockPlacement::playerDirection(90.0F) == Direction::West);
    CHECK(blockPlacement::playerDirection(180.0F) == Direction::North);
    CHECK(blockPlacement::playerDirection(270.0F) == Direction::East);
    // The quarters are centred, so forty-four degrees is still south.
    CHECK(blockPlacement::playerDirection(44.0F) == Direction::South);
    CHECK(blockPlacement::playerDirection(46.0F) == Direction::West);
    CHECK(blockPlacement::playerDirection(-90.0F) == Direction::East);
}

TEST_CASE("the face a player is looking most along") {
    CHECK(blockPlacement::nearestLooking(0.0F, 0.0F) == Direction::South);
    CHECK(blockPlacement::nearestLooking(180.0F, 0.0F) == Direction::North);
    CHECK(blockPlacement::nearestLooking(90.0F, 0.0F) == Direction::West);
    CHECK(blockPlacement::nearestLooking(270.0F, 0.0F) == Direction::East);
    // Looking down beats any yaw.
    CHECK(blockPlacement::nearestLooking(0.0F, 90.0F) == Direction::Down);
    CHECK(blockPlacement::nearestLooking(123.0F, -90.0F) == Direction::Up);
    // And forty-four degrees of pitch does not.
    CHECK(blockPlacement::nearestLooking(0.0F, 44.0F) == Direction::South);
}

TEST_CASE("a log lies along the axis of the face it was put on") {
    CHECK(propertyOf(placed("oak_log", {.face = Direction::Up}), "axis") == "y");
    CHECK(propertyOf(placed("oak_log", {.face = Direction::Down}), "axis") == "y");
    CHECK(propertyOf(placed("oak_log", {.face = Direction::North}), "axis") == "z");
    CHECK(propertyOf(placed("oak_log", {.face = Direction::East}), "axis") == "x");
}

TEST_CASE("a furnace faces whoever put it down and a stair runs away from them") {
    // parity: the general rule is the opposite of the player's own direction.
    CHECK(propertyOf(placed("furnace", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "north");
    // parity: the families that use the direction as it comes.
    CHECK(propertyOf(placed("oak_stairs", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "south");
    CHECK(propertyOf(placed("red_bed", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "south");
    CHECK(propertyOf(placed("oak_fence_gate", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "south");
    CHECK(propertyOf(placed("campfire", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "south");
}

TEST_CASE("an anvil lands sideways to whoever put it down") {
    // parity: `AnvilBlock`, a quarter turn from the player's own direction.
    CHECK(propertyOf(placed("anvil", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "west");
    CHECK(propertyOf(placed("chipped_anvil", {.face = Direction::Up, .yaw = 180.0F}), "facing") == "east");
}

TEST_CASE("a six-way block points away from the player, and a rod at the face it was clicked") {
    CHECK(propertyOf(placed("observer", {.face = Direction::Up, .yaw = 0.0F, .pitch = 0.0F}), "facing") == "north");
    // parity: the handful that take the clicked face instead.
    CHECK(propertyOf(placed("end_rod", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "up");
    CHECK(propertyOf(placed("lightning_rod", {.face = Direction::North, .yaw = 0.0F}), "facing") == "north");
    CHECK(propertyOf(placed("shulker_box", {.face = Direction::Down}), "facing") == "down");
}

TEST_CASE("a button hangs on the face it was clicked and then looks at the player") {
    CHECK(propertyOf(placed("stone_button", {.face = Direction::Up, .yaw = 0.0F}), "face") == "floor");
    CHECK(propertyOf(placed("stone_button", {.face = Direction::Down, .yaw = 0.0F}), "face") == "ceiling");
    CHECK(propertyOf(placed("stone_button", {.face = Direction::North, .yaw = 0.0F}), "face") == "wall");
    // On a wall it looks out of it; on the floor it looks at the player.
    CHECK(propertyOf(placed("stone_button", {.face = Direction::North, .yaw = 0.0F}), "facing") == "north");
    CHECK(propertyOf(placed("stone_button", {.face = Direction::Up, .yaw = 0.0F}), "facing") == "south");
}

TEST_CASE("a wall torch faces the wall it was hung on") {
    CHECK(propertyOf(placed("wall_torch", {.face = Direction::East}), "facing") == "east");
}

TEST_CASE("stairs and slabs go upside down when the click was high or on a ceiling") {
    CHECK(propertyOf(placed("oak_stairs", {.face = Direction::Up, .clickY = 0.9F}), "half") == "bottom");
    CHECK(propertyOf(placed("oak_stairs", {.face = Direction::Down, .clickY = 0.1F}), "half") == "top");
    CHECK(propertyOf(placed("oak_stairs", {.face = Direction::North, .clickY = 0.6F}), "half") == "top");
    CHECK(propertyOf(placed("oak_stairs", {.face = Direction::North, .clickY = 0.4F}), "half") == "bottom");
    // Half way up is not above half way.
    CHECK(propertyOf(placed("oak_stairs", {.face = Direction::North, .clickY = 0.5F}), "half") == "bottom");

    CHECK(propertyOf(placed("oak_slab", {.face = Direction::Up, .clickY = 0.9F}), "type") == "bottom");
    CHECK(propertyOf(placed("oak_slab", {.face = Direction::North, .clickY = 0.6F}), "type") == "top");
}

TEST_CASE("a slab put into a slab of the same kind makes a double one") {
    const std::int32_t bottom = defaultOf("oak_slab");

    CHECK(propertyOf(placed("oak_slab", {.face = Direction::Up, .clickY = 0.1F, .replacing = bottom}), "type") ==
          "double");
    // A slab of another wood is not the same slab.
    CHECK(propertyOf(placed("oak_slab", {.face = Direction::Up, .clickY = 0.1F, .replacing = defaultOf("spruce_slab")}),
                     "type") != "double");
}

TEST_CASE("a block put into water keeps the water") {
    const std::int32_t water = defaultOf("water");

    CHECK(propertyOf(placed("oak_slab", {.face = Direction::Up, .replacing = water}), "waterlogged") == "true");
    CHECK(propertyOf(placed("oak_slab", {.face = Direction::Up, .replacing = 0}), "waterlogged") == "false");

    // And so does one put into something that is merely logged with it.
    const std::optional<std::int32_t> loggedSlab = registry::blockStateDefinitions::stateIdOf(
        block("oak_slab"), {{"type", "bottom"}, {"waterlogged", "true"}}, std::nullopt);
    REQUIRE(loggedSlab.has_value());
    CHECK(propertyOf(placed("oak_fence", {.face = Direction::Up, .replacing = *loggedSlab}), "waterlogged") == "true");
}

TEST_CASE("a rail lies along the axis the player is looking down") {
    CHECK(propertyOf(placed("rail", {.face = Direction::Up, .yaw = 0.0F}), "shape") == "north_south");
    CHECK(propertyOf(placed("rail", {.face = Direction::Up, .yaw = 90.0F}), "shape") == "east_west");
}

TEST_CASE("leaves a player puts down never decay") {
    CHECK(propertyOf(placed("oak_leaves", {.face = Direction::Up}), "persistent") == "true");
}

TEST_CASE("a sign looks back at the player and a skull looks the same way") {
    // parity: sixteen segments, and the sign is turned half a circle.
    CHECK(propertyOf(placed("oak_sign", {.face = Direction::Up, .yaw = 0.0F}), "rotation") == "8");
    CHECK(propertyOf(placed("oak_sign", {.face = Direction::Up, .yaw = 180.0F}), "rotation") == "0");
    CHECK(propertyOf(placed("skeleton_skull", {.face = Direction::Up, .yaw = 0.0F}), "rotation") == "0");
    CHECK(propertyOf(placed("player_head", {.face = Direction::Up, .yaw = 90.0F}), "rotation") == "4");
}

TEST_CASE("a lantern put against a ceiling hangs from it") {
    CHECK(propertyOf(placed("lantern", {.face = Direction::Down}), "hanging") == "true");
    CHECK(propertyOf(placed("lantern", {.face = Direction::Up}), "hanging") == "false");
}

TEST_CASE("candles and eggs pile into the block already there") {
    const std::optional<std::int32_t> one = registry::blockStateDefinitions::stateIdOf(
        block("candle"), {{"candles", "1"}, {"lit", "false"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(one.has_value());

    CHECK(propertyOf(placed("candle", {.face = Direction::Up, .replacing = *one}), "candles") == "2");

    // And the pile stops at the last value the block has.
    const std::optional<std::int32_t> four = registry::blockStateDefinitions::stateIdOf(
        block("candle"), {{"candles", "4"}, {"lit", "false"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(four.has_value());
    CHECK(propertyOf(placed("candle", {.face = Direction::Up, .replacing = *four}), "candles") == "4");
}

TEST_CASE("a mangrove propagule is always planted grown") {
    CHECK(propertyOf(placed("mangrove_propagule", {.face = Direction::Up}), "age") == "4");
}

TEST_CASE("a dripstone points away from the face it was hung on") {
    CHECK(propertyOf(placed("pointed_dripstone", {.face = Direction::Down}), "vertical_direction") == "down");
    CHECK(propertyOf(placed("pointed_dripstone", {.face = Direction::Up}), "vertical_direction") == "up");
    CHECK(propertyOf(placed("pointed_dripstone", {.face = Direction::Up}), "thickness") == "tip");
}

TEST_CASE("a crafter's front is where the player is not") {
    CHECK(propertyOf(placed("crafter", {.face = Direction::Up, .yaw = 0.0F, .pitch = 0.0F}), "orientation") ==
          "north_up");
    // Looking straight down, the top follows the player's own direction.
    CHECK(propertyOf(placed("crafter", {.face = Direction::Up, .yaw = 0.0F, .pitch = -90.0F}), "orientation") ==
          "down_north");
    CHECK(propertyOf(placed("crafter", {.face = Direction::Up, .yaw = 0.0F, .pitch = 90.0F}), "orientation") ==
          "up_south");
}

TEST_CASE("a growing plant head is planted at a random age out of twenty-five") {
    rng::RandomSource random = rng::RandomSource::legacy(4);
    std::int32_t lowest = 25;
    std::int32_t highest = -1;
    for (std::int32_t roll = 0; roll < 500; ++roll) {
        const std::int32_t stateId =
            blockPlacement::stateFor(block("kelp"), defaultOf("kelp"), {.face = Direction::Up}, random);
        const std::int32_t age = std::stoi(propertyOf(stateId, "age"));
        lowest = std::min(lowest, age);
        highest = std::max(highest, age);
    }
    CHECK(lowest == 0);
    CHECK(highest == 24);
}

TEST_CASE("a block with nothing to orient goes down as it is") {
    CHECK(placed("stone", {.face = Direction::Up}) == defaultOf("stone"));
    // And one the registry has never heard of comes back untouched.
    rng::RandomSource random = rng::RandomSource::legacy(1);
    CHECK(blockPlacement::stateFor(block("not_a_block"), 42, {.face = Direction::Up}, random) == 42);
}

TEST_CASE("grass put under snow goes down snowy") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 65, 8, defaultOf("snow_block")));

    CHECK(propertyOf(placed("grass_block", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}),
                     "snowy") == "true");
    CHECK(propertyOf(placed("grass_block", {.face = Direction::Up, .world = &world, .x = 4, .y = 64, .z = 4}),
                     "snowy") == "false");
}

TEST_CASE("a campfire over hay makes the tall smoke, and one in water goes down unlit") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 63, 8, defaultOf("hay_block")));

    CHECK(propertyOf(placed("campfire", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}),
                     "signal_fire") == "true");
    CHECK(propertyOf(placed("campfire", {.face = Direction::Up, .world = &world, .x = 4, .y = 64, .z = 4}),
                     "signal_fire") == "false");
    CHECK(propertyOf(placed("campfire", {.face = Direction::Up, .world = &world, .x = 4, .y = 64, .z = 4}), "lit") ==
          "true");
    CHECK(
        propertyOf(
            placed("campfire",
                   {.face = Direction::Up, .replacing = defaultOf("water"), .world = &world, .x = 4, .y = 64, .z = 4}),
            "lit") == "false");
}

TEST_CASE("a chest beside a single chest facing the same way becomes half of a double one") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::optional<std::int32_t> single = registry::blockStateDefinitions::stateIdOf(
        block("chest"), {{"facing", "north"}, {"type", "single"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(single.has_value());
    // A player looking south puts a chest facing north; the one to its east makes it the left half.
    REQUIRE(world.setBlockState(9, 64, 8, *single));

    const std::int32_t paired =
        placed("chest", {.face = Direction::Up, .yaw = 0.0F, .world = &world, .x = 8, .y = 64, .z = 8});

    CHECK(propertyOf(paired, "facing") == "north");
    CHECK(propertyOf(paired, "type") == "left");
    // With nothing beside it the chest stands alone.
    CHECK(propertyOf(placed("chest", {.face = Direction::Up, .yaw = 0.0F, .world = &world, .x = 4, .y = 64, .z = 4}),
                     "type") == "single");
}

TEST_CASE("a note block takes its instrument from what it lands on") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 63, 8, defaultOf("gold_block")));

    CHECK(propertyOf(placed("note_block", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}),
                     "instrument") == "bell");
    CHECK(propertyOf(placed("note_block", {.face = Direction::Up, .world = &world, .x = 4, .y = 64, .z = 4}),
                     "instrument") == "harp");
}

TEST_CASE("scaffolding counts its distance as it lands") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 63, 8, defaultOf("stone")));

    const std::int32_t onGround =
        placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8});
    CHECK(propertyOf(onGround, "distance") == "0");
    CHECK(propertyOf(onGround, "bottom") == "false");

    REQUIRE(world.setBlockState(8, 64, 8, onGround));
    const std::int32_t beside =
        placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 9, .y = 64, .z = 8});
    CHECK(propertyOf(beside, "distance") == "1");
    CHECK(propertyOf(beside, "bottom") == "true");
}

TEST_CASE("a bell is hung, stood or bolted to one wall or two") {
    Loaded loaded;
    WorldState& world = loaded.state;

    CHECK(propertyOf(placed("bell", {.face = Direction::Down, .world = &world, .x = 8, .y = 64, .z = 8}),
                     "attachment") == "ceiling");
    CHECK(propertyOf(placed("bell", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}), "attachment") ==
          "floor");
    CHECK(propertyOf(placed("bell", {.face = Direction::North, .world = &world, .x = 8, .y = 64, .z = 8}),
                     "attachment") == "single_wall");

    // parity: a sturdy face on both sides bolts it to the pair.
    REQUIRE(world.setBlockState(8, 64, 7, defaultOf("stone")));
    REQUIRE(world.setBlockState(8, 64, 9, defaultOf("stone")));
    const std::int32_t bolted = placed("bell", {.face = Direction::North, .world = &world, .x = 8, .y = 64, .z = 8});
    CHECK(propertyOf(bolted, "attachment") == "double_wall");
    CHECK(propertyOf(bolted, "facing") == "south");
}

TEST_CASE("bamboo planted on the ground is a sapling and on bamboo it copies how grown it is") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 63, 8, defaultOf("dirt")));

    CHECK(placed("bamboo", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}) ==
          defaultOf("bamboo_sapling"));

    REQUIRE(world.setBlockState(8, 63, 8, defaultOf("bamboo_sapling")));
    CHECK(propertyOf(placed("bamboo", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}), "age") ==
          "0");

    const std::optional<std::int32_t> grown = registry::blockStateDefinitions::stateIdOf(
        block("bamboo"), {{"age", "1"}, {"leaves", "none"}, {"stage", "0"}}, std::nullopt);
    REQUIRE(grown.has_value());
    REQUIRE(world.setBlockState(8, 63, 8, *grown));
    CHECK(propertyOf(placed("bamboo", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}), "age") ==
          "1");
}

namespace {

/// The hinge a door gets at (8, 64, 8), with the four squares beside it filled in as asked.
/// `sides` is left-lower, left-upper, right-lower, right-upper, counted from the player's own
/// direction: looking south, the door's left is east.
[[nodiscard]] std::string hingeOf(float yaw, float clickX, float clickZ, std::array<bool, 4> sides,
                                  const std::string& leftBlock = "stone", const std::string& rightBlock = "stone") {
    Loaded loaded;
    WorldState& world = loaded.state;
    // north, east, south, west, so a quarter turn clockwise is one step along.
    constexpr std::array<std::array<std::int32_t, 2>, 4> kSteps{{{0, -1}, {1, 0}, {0, 1}, {-1, 0}}};
    const auto quarterOf = [](Direction face) {
        switch (face) {
        case Direction::North:
            return 0;
        case Direction::East:
            return 1;
        case Direction::South:
            return 2;
        default:
            return 3;
        }
    };
    const int towards = quarterOf(blockPlacement::playerDirection(yaw));
    const std::array<std::int32_t, 2> left = kSteps[static_cast<std::size_t>((towards + 3) % 4)];
    const std::array<std::int32_t, 2> right = kSteps[static_cast<std::size_t>((towards + 1) % 4)];
    for (std::size_t index = 0; index < sides.size(); ++index) {
        if (!sides[index]) {
            continue;
        }
        const std::array<std::int32_t, 2>& side = index < 2 ? left : right;
        const std::string& path = index < 2 ? leftBlock : rightBlock;
        const std::int32_t y = index % 2 == 0 ? 64 : 65;
        REQUIRE(world.setBlockState(8 + side[0], y, 8 + side[1], defaultOf(path)));
    }
    return propertyOf(placed("oak_door", {.face = Direction::Up,
                                          .clickX = clickX,
                                          .clickZ = clickZ,
                                          .yaw = yaw,
                                          .world = &world,
                                          .x = 8,
                                          .y = 64,
                                          .z = 8}),
                      "hinge");
}

} // namespace

TEST_CASE("a door hangs away from the solid blocks beside it") {
    // parity: `DoorBlock.getHinge` counts the four squares, each solid one a point for its side.
    // Looking south, the door's left is east; every case below clicks where the fallback would
    // answer the other way, so only the count can be deciding it.
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {true, false, false, false}) == "left");
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {false, true, false, false}) == "left");
    CHECK(hingeOf(0.0F, 0.1F, 0.5F, {false, false, true, false}) == "right");
    CHECK(hingeOf(0.0F, 0.1F, 0.5F, {false, false, false, true}) == "right");

    // One point either way cancels out and the click decides again.
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {true, false, true, false}) == "left");
    CHECK(hingeOf(0.0F, 0.1F, 0.5F, {true, false, true, false}) == "right");

    // A side worth one against a side worth two, which is where the count itself has to be right.
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {false, true, true, true}) == "right");
    // And a side worth two beats a side worth one.
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {true, false, true, true}) == "right");
    CHECK(hingeOf(0.0F, 0.1F, 0.5F, {true, true, false, true}) == "left");

    // A block that is not a full cube is worth nothing at all.
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {false, false, true, true}, "stone", "oak_slab") == "left");
}

TEST_CASE("a door beside another door hangs the other way") {
    // parity: only the lower half of a door counts, and only when there is no door on the other side.
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {true, false, false, false}, "oak_door") == "right");
    CHECK(hingeOf(0.0F, 0.1F, 0.5F, {false, false, true, false}, "stone", "oak_door") == "left");
    // A door on both sides settles nothing, so the click decides.
    CHECK(hingeOf(0.0F, 0.9F, 0.5F, {true, false, true, false}, "oak_door", "oak_door") == "left");
    // The upper half of a door is not a door for this, and it is not a full cube either.
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::optional<std::int32_t> upper = registry::blockStateDefinitions::stateIdOf(
        block("oak_door"),
        {{"facing", "north"}, {"half", "upper"}, {"hinge", "left"}, {"open", "false"}, {"powered", "false"}},
        std::nullopt);
    REQUIRE(upper.has_value());
    REQUIRE(world.setBlockState(9, 64, 8, *upper));
    CHECK(propertyOf(
              placed("oak_door",
                     {.face = Direction::Up, .clickX = 0.9F, .yaw = 0.0F, .world = &world, .x = 8, .y = 64, .z = 8}),
              "hinge") == "left");
}

TEST_CASE("with nothing to go by a door hangs on the half that was clicked") {
    // parity: the test is on the click coordinate across the direction the player faces, and the
    // half-way point counts as clicked.
    // Facing north or south, only the click across x is read, whatever the other one says.
    CHECK(hingeOf(0.0F, 0.9F, 0.1F, {}) == "left");
    CHECK(hingeOf(0.0F, 0.9F, 0.9F, {}) == "left");
    CHECK(hingeOf(0.0F, 0.5F, 0.1F, {}) == "left");
    CHECK(hingeOf(0.0F, 0.1F, 0.1F, {}) == "right");
    CHECK(hingeOf(0.0F, 0.1F, 0.9F, {}) == "right");
    CHECK(hingeOf(180.0F, 0.1F, 0.1F, {}) == "left");
    CHECK(hingeOf(180.0F, 0.1F, 0.9F, {}) == "left");
    CHECK(hingeOf(180.0F, 0.5F, 0.9F, {}) == "left");
    CHECK(hingeOf(180.0F, 0.9F, 0.1F, {}) == "right");
    CHECK(hingeOf(180.0F, 0.9F, 0.9F, {}) == "right");
    // Facing east or west it is the other way round.
    CHECK(hingeOf(90.0F, 0.1F, 0.9F, {}) == "left");
    CHECK(hingeOf(90.0F, 0.9F, 0.9F, {}) == "left");
    CHECK(hingeOf(90.0F, 0.1F, 0.5F, {}) == "left");
    CHECK(hingeOf(90.0F, 0.1F, 0.1F, {}) == "right");
    CHECK(hingeOf(90.0F, 0.9F, 0.1F, {}) == "right");
    CHECK(hingeOf(270.0F, 0.1F, 0.1F, {}) == "left");
    CHECK(hingeOf(270.0F, 0.9F, 0.1F, {}) == "left");
    CHECK(hingeOf(270.0F, 0.9F, 0.5F, {}) == "left");
    CHECK(hingeOf(270.0F, 0.1F, 0.9F, {}) == "right");
    CHECK(hingeOf(270.0F, 0.9F, 0.9F, {}) == "right");
}

TEST_CASE("a box that only touches a block does not obstruct it") {
    const std::int32_t stone = defaultOf("stone");
    // A block at (3, 70, 5) fills x 3..4, y 70..71, z 5..6, and touching a face is not overlapping it.
    const auto blocked = [stone](blockPlacement::Box box) {
        return blockPlacement::isObstructed(stone, 3, 70, 5, std::span{&box, 1});
    };

    CHECK(blocked({3.5, 70.5, 5.5, 3.6, 70.6, 5.6}));
    CHECK_FALSE(blocked({2.5, 70.5, 5.5, 3.0, 70.6, 5.6}));
    CHECK(blocked({2.5, 70.5, 5.5, 3.01, 70.6, 5.6}));
    CHECK_FALSE(blocked({4.0, 70.5, 5.5, 4.5, 70.6, 5.6}));
    CHECK_FALSE(blocked({3.5, 69.5, 5.5, 3.6, 70.0, 5.6}));
    CHECK_FALSE(blocked({3.5, 71.0, 5.5, 3.6, 71.5, 5.6}));
    CHECK_FALSE(blocked({3.5, 70.5, 4.5, 3.6, 70.6, 5.0}));
    CHECK_FALSE(blocked({3.5, 70.5, 6.0, 3.6, 70.6, 6.5}));
}

TEST_CASE("a block of more than one box is asked about every one of them") {
    // parity: bottom stairs are the slab below plus the step on the half the block faces, which for
    // the default state is the north half: x 3..4, y 70.5..71, z 5..5.5.
    const std::int32_t stairs = defaultOf("oak_stairs");
    const auto blocked = [stairs](blockPlacement::Box box) {
        return blockPlacement::isObstructed(stairs, 3, 70, 5, std::span{&box, 1});
    };

    // The slab, on both halves of the block.
    CHECK(blocked({3.6, 70.1, 5.6, 3.9, 70.4, 5.9}));
    CHECK(blocked({3.1, 70.1, 5.1, 3.4, 70.4, 5.4}));
    // The step, which only the second box covers.
    CHECK(blocked({3.6, 70.6, 5.1, 3.9, 70.9, 5.4}));
    CHECK(blocked({3.1, 70.6, 5.1, 3.4, 70.9, 5.4}));
    // And the quarter neither box reaches.
    CHECK_FALSE(blocked({3.4, 70.5, 5.6, 3.6, 70.9, 5.9}));
    CHECK_FALSE(blocked({3.4, 70.6, 5.6, 3.6, 70.9, 5.9}));

    // Nothing to be in the way of, and nothing to be in the way.
    CHECK_FALSE(blockPlacement::isObstructed(stairs, 3, 70, 5, {}));
    const blockPlacement::Box inside{3.5, 70.1, 5.5, 3.6, 70.2, 5.6};
    CHECK_FALSE(blockPlacement::isObstructed(defaultOf("torch"), 3, 70, 5, std::span{&inside, 1}));
}

TEST_CASE("an anvil has a gap under its own top that nothing fills") {
    // The numbers below are the anvil's own collision boxes: the pillar is the middle of the block
    // from a third of the way up, and the top sits above five eighths, so the corner between them is
    // empty. It is the one shape where a box's floor is not the floor of the block.
    const std::int32_t anvil = defaultOf("anvil");
    const auto blocked = [anvil](blockPlacement::Box box) {
        return blockPlacement::isObstructed(anvil, 3, 70, 5, std::span{&box, 1});
    };

    CHECK_FALSE(blocked({3.2, 70.45, 5.45, 3.3, 70.55, 5.55}));
    CHECK(blocked({3.2, 70.7, 5.45, 3.3, 70.8, 5.55}));
    CHECK(blocked({3.45, 70.45, 5.45, 3.55, 70.55, 5.55}));
}

TEST_CASE("a crouching player is shorter") {
    const blockPlacement::Box standing = blockPlacement::playerBox(0.0, 0.0, 0.0, false);
    const blockPlacement::Box sneaking = blockPlacement::playerBox(0.0, 0.0, 0.0, true);

    CHECK(standing[0] == doctest::Approx(-0.3));
    CHECK(standing[3] == doctest::Approx(0.3));
    CHECK(standing[4] == doctest::Approx(1.8));
    CHECK(sneaking[4] == doctest::Approx(1.5));
}

TEST_CASE("the face a player is looking along flips at the diagonal") {
    // parity: the pitch is asked first, so it beats the yaw as soon as it is the longer of the two.
    CHECK(blockPlacement::nearestLooking(44.0F, 0.0F) == Direction::South);
    CHECK(blockPlacement::nearestLooking(46.0F, 0.0F) == Direction::West);
    CHECK(blockPlacement::nearestLooking(-44.0F, 0.0F) == Direction::South);
    CHECK(blockPlacement::nearestLooking(-46.0F, 0.0F) == Direction::East);
    CHECK(blockPlacement::nearestLooking(134.0F, 0.0F) == Direction::West);
    CHECK(blockPlacement::nearestLooking(136.0F, 0.0F) == Direction::North);
    CHECK(blockPlacement::nearestLooking(224.0F, 0.0F) == Direction::North);
    CHECK(blockPlacement::nearestLooking(226.0F, 0.0F) == Direction::East);
    CHECK(blockPlacement::nearestLooking(0.0F, 44.0F) == Direction::South);
    CHECK(blockPlacement::nearestLooking(0.0F, 46.0F) == Direction::Down);
    CHECK(blockPlacement::nearestLooking(0.0F, -46.0F) == Direction::Up);
    CHECK(blockPlacement::nearestLooking(90.0F, 44.0F) == Direction::West);
    CHECK(blockPlacement::nearestLooking(90.0F, 46.0F) == Direction::Down);
    CHECK(blockPlacement::nearestLooking(90.0F, -46.0F) == Direction::Up);
}

TEST_CASE("a sign turns in sixteenths") {
    // parity: `RotationSegment`, sixteen segments with the sign turned half a circle from the player.
    CHECK(propertyOf(placed("oak_sign", {.face = Direction::Up, .yaw = 22.5F}), "rotation") == "9");
    CHECK(propertyOf(placed("oak_sign", {.face = Direction::Up, .yaw = 45.0F}), "rotation") == "10");
    CHECK(propertyOf(placed("skeleton_skull", {.face = Direction::Up, .yaw = 22.5F}), "rotation") == "1");
    CHECK(propertyOf(placed("skeleton_skull", {.face = Direction::Up, .yaw = -22.5F}), "rotation") == "15");
}

TEST_CASE("scaffolding counts up to seven and no further") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(1, 63, 8, defaultOf("stone")));

    // A run out from the ground: each step is one further, and the count stops at seven.
    std::int32_t previous = placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 1, .y = 64, .z = 8});
    REQUIRE(world.setBlockState(1, 64, 8, previous));
    for (std::int32_t step = 1; step <= 8; ++step) {
        previous = placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 1 + step, .y = 64, .z = 8});
        CHECK(propertyOf(previous, "distance") == std::to_string(std::min(step, 7)));
        CHECK(propertyOf(previous, "bottom") == "true");
        REQUIRE(world.setBlockState(1 + step, 64, 8, previous));
    }

    // Scaffolding straight below is what makes a piece not the bottom of its run.
    REQUIRE(world.setBlockState(12, 63, 8, previous));
    CHECK(propertyOf(placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 12, .y = 64, .z = 8}),
                     "bottom") == "false");
}

TEST_CASE("only a single chest of the same kind pairs with a new one") {
    const auto typeBeside = [](const std::string& path, const std::string& type) {
        Loaded loaded;
        WorldState& world = loaded.state;
        const std::optional<std::int32_t> beside = registry::blockStateDefinitions::stateIdOf(
            block(path), {{"facing", "north"}, {"type", type}, {"waterlogged", "false"}}, std::nullopt);
        REQUIRE(beside.has_value());
        REQUIRE(world.setBlockState(9, 64, 8, *beside));
        return propertyOf(
            placed("chest", {.face = Direction::Up, .yaw = 0.0F, .world = &world, .x = 8, .y = 64, .z = 8}), "type");
    };

    CHECK(typeBeside("chest", "single") == "left");
    // Half of a pair is already spoken for, and a trapped chest is another block entirely.
    CHECK(typeBeside("chest", "left") == "single");
    CHECK(typeBeside("chest", "right") == "single");
    CHECK(typeBeside("trapped_chest", "single") == "single");
}

TEST_CASE("bamboo reads how grown the stalk under it is, and the one over it when there is none") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto stalk = [](const std::string& age) {
        const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
            block("bamboo"), {{"age", age}, {"leaves", "none"}, {"stage", "0"}}, std::nullopt);
        REQUIRE(stateId.has_value());
        return *stateId;
    };

    // A stalk below hands its own age down to one or zero, never anything else.
    REQUIRE(world.setBlockState(8, 63, 8, stalk("0")));
    CHECK(propertyOf(placed("bamboo", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}), "age") ==
          "0");
    REQUIRE(world.setBlockState(8, 63, 8, stalk("1")));
    CHECK(propertyOf(placed("bamboo", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8}), "age") ==
          "1");

    // With no stalk below, a stalk above is read instead, and its age is taken as it is.
    REQUIRE(world.setBlockState(4, 63, 8, defaultOf("dirt")));
    REQUIRE(world.setBlockState(4, 65, 8, stalk("1")));
    CHECK(propertyOf(placed("bamboo", {.face = Direction::Up, .world = &world, .x = 4, .y = 64, .z = 8}), "age") ==
          "1");
    REQUIRE(world.setBlockState(4, 65, 8, stalk("0")));
    CHECK(propertyOf(placed("bamboo", {.face = Direction::Up, .world = &world, .x = 4, .y = 64, .z = 8}), "age") ==
          "0");
}

TEST_CASE("a bell is bolted to a pair of walls on whichever axis it was hung across") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(7, 64, 8, defaultOf("stone")));
    REQUIRE(world.setBlockState(9, 64, 8, defaultOf("stone")));

    // parity: hung on an east or west wall, the pair it looks for is the one across x.
    const std::int32_t bolted = placed("bell", {.face = Direction::East, .world = &world, .x = 8, .y = 64, .z = 8});
    CHECK(propertyOf(bolted, "attachment") == "double_wall");
    CHECK(propertyOf(bolted, "facing") == "west");
    // The same walls say nothing about a bell hung across z.
    CHECK(propertyOf(placed("bell", {.face = Direction::North, .world = &world, .x = 8, .y = 64, .z = 8}),
                     "attachment") == "single_wall");
}

TEST_CASE("scaffolding standing on scaffolding counts from the piece below it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::optional<std::int32_t> three = registry::blockStateDefinitions::stateIdOf(
        block("scaffolding"), {{"distance", "3"}, {"bottom", "false"}, {"waterlogged", "false"}}, std::nullopt);
    REQUIRE(three.has_value());
    REQUIRE(world.setBlockState(8, 63, 8, *three));

    // parity: the piece below hands its own count over, while a piece beside it would add one.
    const std::int32_t above = placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 8, .y = 64, .z = 8});
    CHECK(propertyOf(above, "distance") == "3");
    CHECK(propertyOf(above, "bottom") == "false");

    REQUIRE(world.setBlockState(4, 64, 8, *three));
    CHECK(propertyOf(placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 5, .y = 64, .z = 8}),
                     "distance") == "4");
}

TEST_CASE("a block put down against an unloaded chunk is put down against nothing") {
    Loaded loaded;
    WorldState& world = loaded.state;

    // parity: scaffolding on a sturdy face counts zero, and there is no face out here at all.
    const std::int32_t alone =
        placed("scaffolding", {.face = Direction::Up, .world = &world, .x = 100, .y = 64, .z = 100});
    CHECK(propertyOf(alone, "distance") == "7");
    CHECK(propertyOf(alone, "bottom") == "true");
}
