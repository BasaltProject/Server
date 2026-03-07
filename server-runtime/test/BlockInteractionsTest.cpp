#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/runtime/world/BlockInteractions.hpp>

#include <doctest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

using Properties = std::map<std::string, std::string>;

[[nodiscard]] std::int32_t stateOf(const std::string& path, const Properties& properties = {}) {
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
    const Properties properties = registry::blockStateDefinitions::propertiesOf(*stateId);
    const auto found = properties.find(key);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

/// A whole door, both halves, at the given position with the lower half at `y`.
void placeDoor(WorldState& world, const std::string& path, std::int32_t x, std::int32_t y, std::int32_t z,
               const std::string& open = "false") {
    REQUIRE(world.setBlockState(x, y, z, stateOf(path, {{"half", "lower"}, {"open", open}})));
    REQUIRE(world.setBlockState(x, y + 1, z, stateOf(path, {{"half", "upper"}, {"open", open}})));
}

} // namespace

TEST_CASE("a hand moves levers, buttons, doors, trapdoors and fence gates and nothing else") {
    CHECK(blockInteractions::isToggleable(stateOf("lever")));
    CHECK(blockInteractions::isToggleable(stateOf("oak_button")));
    CHECK(blockInteractions::isToggleable(stateOf("stone_button")));
    CHECK(blockInteractions::isToggleable(stateOf("oak_door")));
    CHECK(blockInteractions::isToggleable(stateOf("oak_trapdoor")));
    CHECK(blockInteractions::isToggleable(stateOf("oak_fence_gate")));
    CHECK(blockInteractions::isToggleable(stateOf("copper_door")));
    // parity: `BlockSetType.IRON.canOpenByHand` is false, so a hand does nothing to either.
    CHECK_FALSE(blockInteractions::isToggleable(stateOf("iron_door")));
    CHECK_FALSE(blockInteractions::isToggleable(stateOf("iron_trapdoor")));
    CHECK_FALSE(blockInteractions::isToggleable(stateOf("stone")));
    CHECK_FALSE(blockInteractions::isToggleable(stateOf("air")));
    CHECK_FALSE(blockInteractions::isToggleable(stateOf("repeater")));
}

TEST_CASE("only a lever and a button tell the neighbours when a hand moves them") {
    // parity: `LeverBlock` and `ButtonBlock` write with flag three. `DoorBlock` and `FenceGateBlock`
    // write with ten and `TrapDoorBlock` with two, and none of those carries `UPDATE_NEIGHBORS`: a
    // door opened by hand redraws the shapes around it and tells nobody at all.
    CHECK(blockInteractions::handToggleTellsNeighbours(stateOf("lever")));
    CHECK(blockInteractions::handToggleTellsNeighbours(stateOf("oak_button")));
    CHECK(blockInteractions::handToggleTellsNeighbours(stateOf("stone_button")));
    CHECK_FALSE(blockInteractions::handToggleTellsNeighbours(stateOf("oak_door")));
    CHECK_FALSE(blockInteractions::handToggleTellsNeighbours(stateOf("oak_trapdoor")));
    CHECK_FALSE(blockInteractions::handToggleTellsNeighbours(stateOf("oak_fence_gate")));
    CHECK_FALSE(blockInteractions::handToggleTellsNeighbours(stateOf("copper_door")));
    // And what a hand cannot move at all does not tell anybody either.
    CHECK_FALSE(blockInteractions::handToggleTellsNeighbours(stateOf("iron_door")));
    CHECK_FALSE(blockInteractions::handToggleTellsNeighbours(stateOf("stone")));
}

TEST_CASE("the four components a click sets up") {
    CHECK(blockInteractions::isCycleable(stateOf("repeater")));
    CHECK(blockInteractions::isCycleable(stateOf("comparator")));
    // parity: `DaylightDetectorBlock.useWithoutItem` cycles `inverted` and
    // `RedStoneWireBlock.useWithoutItem` turns a cross into a dot. Both are set up by clicking them,
    // and the redstone bench cannot press anything, so neither was reachable at all.
    CHECK(blockInteractions::isCycleable(stateOf("daylight_detector")));
    CHECK(blockInteractions::isCycleable(stateOf("redstone_wire")));
    CHECK_FALSE(blockInteractions::isCycleable(stateOf("lever")));
    CHECK_FALSE(blockInteractions::isCycleable(stateOf("oak_door")));
    CHECK_FALSE(blockInteractions::isCycleable(stateOf("stone")));
}

TEST_CASE("a click turns a daylight detector over") {
    Loaded loaded;
    REQUIRE(loaded.state.setBlockState(8, 64, 8, stateOf("daylight_detector", {{"inverted", "false"}})));
    REQUIRE(blockInteractions::cycle(loaded.state, 8, 64, 8).has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "inverted") == "true");
    REQUIRE(blockInteractions::cycle(loaded.state, 8, 64, 8).has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "inverted") == "false");
}

TEST_CASE("a click turns a dust cross into a dot and back") {
    // parity: `RedStoneWireBlock.useWithoutItem` only answers for a cross or a dot, and redraws with
    // `getConnectionState` afterwards, so with nothing around the cross becomes a dot and the dot
    // becomes a cross again.
    Loaded loaded;
    const Properties cross{{"north", "side"}, {"south", "side"}, {"east", "side"}, {"west", "side"}};
    REQUIRE(loaded.state.setBlockState(8, 64, 8, stateOf("redstone_wire", cross)));
    REQUIRE(blockInteractions::cycle(loaded.state, 8, 64, 8).has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "north") == "none");
    REQUIRE(blockInteractions::cycle(loaded.state, 8, 64, 8).has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "north") == "side");
    // A dust already drawn as a line is left alone: it is neither a cross nor a dot. The two sides
    // that are not part of the line have to be named: what a state id is not asked for comes from the
    // block's **first** state, and a dust's first state has all four sides up.
    REQUIRE(loaded.state.setBlockState(
        8, 64, 10,
        stateOf("redstone_wire", {{"north", "side"}, {"south", "side"}, {"east", "none"}, {"west", "none"}})));
    CHECK_FALSE(blockInteractions::cycle(loaded.state, 8, 64, 10).has_value());
}

TEST_CASE("a fence gate opened from the wrong side swings out of the way") {
    // parity: `FenceGateBlock.useWithoutItem`. Only on the way open, and only when the gate faces
    // **away** from whoever opened it.
    Loaded loaded;
    const auto gate = [&](const std::string& facing, const std::string& open) {
        return stateOf("oak_fence_gate", {{"facing", facing}, {"open", open}});
    };
    // Yaw zero is looking south. A gate facing north is facing away, so it turns.
    REQUIRE(loaded.state.setBlockState(8, 64, 8, gate("north", "false")));
    REQUIRE(blockInteractions::toggle(loaded.state, 8, 64, 8, 0.0F).has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 8, "facing") == "south");
    CHECK(propertyAt(loaded.state, 8, 64, 8, "open") == "true");

    // One facing the player already is left where it is.
    REQUIRE(loaded.state.setBlockState(8, 64, 10, gate("south", "false")));
    REQUIRE(blockInteractions::toggle(loaded.state, 8, 64, 10, 0.0F).has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 10, "facing") == "south");

    // And one that is closing never turns, whichever way it faces.
    REQUIRE(loaded.state.setBlockState(8, 64, 12, gate("north", "true")));
    REQUIRE(blockInteractions::toggle(loaded.state, 8, 64, 12, 0.0F).has_value());
    CHECK(propertyAt(loaded.state, 8, 64, 12, "facing") == "north");
    CHECK(propertyAt(loaded.state, 8, 64, 12, "open") == "false");
}

TEST_CASE("a state id no block owns answers no to everything") {
    constexpr std::int32_t unknown = 1'000'000;
    REQUIRE(registry::blockStateDefinitions::blockOf(unknown) == nullptr);
    CHECK_FALSE(blockInteractions::isToggleable(unknown));
    CHECK_FALSE(blockInteractions::isCycleable(unknown));
    CHECK_FALSE(blockInteractions::pressedTicks(unknown).has_value());
    CHECK_FALSE(blockInteractions::redstoneSound(unknown, true).has_value());
}

TEST_CASE("both halves of a door swing together whichever one is clicked") {
    Loaded world;
    placeDoor(world.state, "oak_door", 3, 64, 3);

    const std::optional<blockInteractions::BlockToggle> opened = blockInteractions::toggle(world.state, 3, 64, 3);
    REQUIRE(opened.has_value());
    CHECK(opened->sound == "minecraft:block.wooden_door.open");
    REQUIRE(opened->changes.size() == 2);
    CHECK(opened->changes[0].y == 64);
    CHECK(opened->changes[1].y == 65);
    CHECK(opened->changes[0].x == 3);
    CHECK(opened->changes[0].z == 3);
    CHECK(propertyAt(world.state, 3, 64, 3, "open") == "true");
    CHECK(propertyAt(world.state, 3, 65, 3, "open") == "true");

    // The upper half closes the pair again, reaching down instead of up.
    const std::optional<blockInteractions::BlockToggle> closed = blockInteractions::toggle(world.state, 3, 65, 3);
    REQUIRE(closed.has_value());
    CHECK(closed->sound == "minecraft:block.wooden_door.close");
    REQUIRE(closed->changes.size() == 2);
    CHECK(closed->changes[0].y == 65);
    CHECK(closed->changes[1].y == 64);
    CHECK(propertyAt(world.state, 3, 64, 3, "open") == "false");
    CHECK(propertyAt(world.state, 3, 65, 3, "open") == "false");
}

TEST_CASE("a half door moves the half that is there and nothing else") {
    Loaded world;
    REQUIRE(world.state.setBlockState(4, 64, 4, stateOf("oak_door", {{"half", "lower"}, {"open", "false"}})));
    REQUIRE(world.state.setBlockState(4, 65, 4, stateOf("spruce_door", {{"half", "upper"}, {"open", "false"}})));

    const std::optional<blockInteractions::BlockToggle> moved = blockInteractions::toggle(world.state, 4, 64, 4);
    REQUIRE(moved.has_value());
    REQUIRE(moved->changes.size() == 1);
    CHECK(moved->changes[0].y == 64);
    // The spruce half is a different block, so it stays as it was.
    CHECK(propertyAt(world.state, 4, 65, 4, "open") == "false");
}

TEST_CASE("a trapdoor and a fence gate move alone") {
    Loaded world;
    REQUIRE(world.state.setBlockState(5, 64, 5, stateOf("oak_trapdoor", {{"open", "false"}})));
    REQUIRE(world.state.setBlockState(5, 65, 5, stateOf("oak_fence_gate", {{"open", "false"}})));

    const std::optional<blockInteractions::BlockToggle> trapdoor = blockInteractions::toggle(world.state, 5, 64, 5);
    REQUIRE(trapdoor.has_value());
    CHECK(trapdoor->sound == "minecraft:block.wooden_trapdoor.open");
    CHECK(trapdoor->changes.size() == 1);

    const std::optional<blockInteractions::BlockToggle> gate = blockInteractions::toggle(world.state, 5, 65, 5);
    REQUIRE(gate.has_value());
    // parity: a plain fence gate has no wood in its sound name.
    CHECK(gate->sound == "minecraft:block.fence_gate.open");
    CHECK(gate->changes.size() == 1);
    CHECK(propertyAt(world.state, 5, 65, 5, "open") == "true");
}

TEST_CASE("a lever clicks and flips its power") {
    Loaded world;
    REQUIRE(world.state.setBlockState(6, 64, 6, stateOf("lever", {{"powered", "false"}})));

    const std::optional<blockInteractions::BlockToggle> on = blockInteractions::toggle(world.state, 6, 64, 6);
    REQUIRE(on.has_value());
    CHECK(on->sound == "minecraft:block.lever.click");
    CHECK(propertyAt(world.state, 6, 64, 6, "powered") == "true");

    const std::optional<blockInteractions::BlockToggle> off = blockInteractions::toggle(world.state, 6, 64, 6);
    REQUIRE(off.has_value());
    // The lever's click is the same both ways round.
    CHECK(off->sound == "minecraft:block.lever.click");
    CHECK(propertyAt(world.state, 6, 64, 6, "powered") == "false");
}

TEST_CASE("a button goes down once and stays down until its own tick") {
    Loaded world;
    REQUIRE(world.state.setBlockState(7, 64, 7, stateOf("stone_button", {{"powered", "false"}})));

    const std::optional<blockInteractions::BlockToggle> pressed = blockInteractions::toggle(world.state, 7, 64, 7);
    REQUIRE(pressed.has_value());
    CHECK(pressed->sound == "minecraft:block.stone_button.click_on");
    CHECK(propertyAt(world.state, 7, 64, 7, "powered") == "true");

    // parity: `ButtonBlock.useWithoutItem` consumes the click and changes nothing.
    CHECK_FALSE(blockInteractions::toggle(world.state, 7, 64, 7).has_value());
    CHECK(propertyAt(world.state, 7, 64, 7, "powered") == "true");

    const std::optional<blockInteractions::BlockRelease> released = blockInteractions::release(world.state, 7, 64, 7);
    REQUIRE(released.has_value());
    CHECK(released->sound == "minecraft:block.stone_button.click_off");
    CHECK(released->stateId == stateOf("stone_button", {{"powered", "false"}}));
    CHECK(propertyAt(world.state, 7, 64, 7, "powered") == "false");

    // A button already up has nothing to release.
    CHECK_FALSE(blockInteractions::release(world.state, 7, 64, 7).has_value());
}

TEST_CASE("only a button releases, and only a button counts its ticks") {
    Loaded world;
    REQUIRE(world.state.setBlockState(8, 64, 8, stateOf("lever", {{"powered", "true"}})));
    CHECK_FALSE(blockInteractions::release(world.state, 8, 64, 8).has_value());
    CHECK(propertyAt(world.state, 8, 64, 8, "powered") == "true");

    CHECK(blockInteractions::pressedTicks(stateOf("stone_button")) == 20);
    CHECK(blockInteractions::pressedTicks(stateOf("polished_blackstone_button")) == 20);
    CHECK(blockInteractions::pressedTicks(stateOf("oak_button")) == 30);
    CHECK(blockInteractions::pressedTicks(stateOf("bamboo_button")) == 30);
    CHECK(blockInteractions::pressedTicks(stateOf("cherry_button")) == 30);
    CHECK_FALSE(blockInteractions::pressedTicks(stateOf("lever")).has_value());
    CHECK_FALSE(blockInteractions::pressedTicks(stateOf("oak_door")).has_value());
    CHECK_FALSE(blockInteractions::pressedTicks(stateOf("stone")).has_value());
}

TEST_CASE("a button's click follows its set rather than its other sounds") {
    Loaded world;
    const auto press = [&world](const std::string& path) {
        REQUIRE(world.state.setBlockState(9, 64, 9, stateOf(path, {{"powered", "false"}})));
        const std::optional<blockInteractions::BlockToggle> toggled = blockInteractions::toggle(world.state, 9, 64, 9);
        REQUIRE(toggled.has_value());
        return toggled->sound;
    };
    CHECK(press("stone_button") == "minecraft:block.stone_button.click_on");
    CHECK(press("polished_blackstone_button") == "minecraft:block.stone_button.click_on");
    CHECK(press("oak_button") == "minecraft:block.wooden_button.click_on");
    CHECK(press("bamboo_button") == "minecraft:block.bamboo_wood_button.click_on");
    CHECK(press("cherry_button") == "minecraft:block.cherry_wood_button.click_on");
    CHECK(press("crimson_button") == "minecraft:block.nether_wood_button.click_on");
    CHECK(press("warped_button") == "minecraft:block.nether_wood_button.click_on");
}

TEST_CASE("a repeater walks its delay round and says nothing") {
    Loaded world;
    REQUIRE(world.state.setBlockState(10, 64, 10, stateOf("repeater", {{"delay", "1"}})));

    for (const std::string expected : {"2", "3", "4", "1"}) {
        const std::optional<blockInteractions::BlockCycle> turned = blockInteractions::cycle(world.state, 10, 64, 10);
        REQUIRE(turned.has_value());
        CHECK_FALSE(turned->sound.has_value());
        CHECK(turned->pitch == 1.0F);
        CHECK(turned->volume == 1.0F);
        CHECK(turned->stateId == world.state.blockState(10, 64, 10).value());
        CHECK(propertyAt(world.state, 10, 64, 10, "delay") == expected);
    }
}

TEST_CASE("a comparator flips its mode and clicks at the pitch that says which way") {
    Loaded world;
    REQUIRE(world.state.setBlockState(11, 64, 11, stateOf("comparator", {{"mode", "compare"}})));

    const std::optional<blockInteractions::BlockCycle> subtract = blockInteractions::cycle(world.state, 11, 64, 11);
    REQUIRE(subtract.has_value());
    REQUIRE(subtract->sound.has_value());
    CHECK(*subtract->sound == "block.comparator.click");
    CHECK(subtract->pitch == 0.55F);
    CHECK(subtract->volume == 0.3F);
    CHECK(propertyAt(world.state, 11, 64, 11, "mode") == "subtract");

    const std::optional<blockInteractions::BlockCycle> compare = blockInteractions::cycle(world.state, 11, 64, 11);
    REQUIRE(compare.has_value());
    CHECK(compare->pitch == 0.5F);
    CHECK(compare->volume == 0.3F);
    CHECK(propertyAt(world.state, 11, 64, 11, "mode") == "compare");
}

TEST_CASE("nothing else cycles, and nothing at all happens off the map") {
    Loaded world;
    REQUIRE(world.state.setBlockState(12, 64, 12, stateOf("lever")));
    CHECK_FALSE(blockInteractions::cycle(world.state, 12, 64, 12).has_value());
    CHECK_FALSE(blockInteractions::toggle(world.state, 12, 65, 12).has_value());

    // An unloaded chunk answers nothing rather than generating one.
    CHECK_FALSE(blockInteractions::toggle(world.state, 800, 64, 800).has_value());
    CHECK_FALSE(blockInteractions::cycle(world.state, 800, 64, 800).has_value());
    CHECK_FALSE(blockInteractions::release(world.state, 800, 64, 800).has_value());
}

TEST_CASE("redstone opens the three that make a noise and nothing else") {
    const auto sound = [](const std::string& path, bool open) {
        return blockInteractions::redstoneSound(stateOf(path), open);
    };

    CHECK(sound("oak_door", true) == "minecraft:block.wooden_door.open");
    CHECK(sound("oak_door", false) == "minecraft:block.wooden_door.close");
    CHECK(sound("oak_trapdoor", true) == "minecraft:block.wooden_trapdoor.open");
    CHECK(sound("oak_fence_gate", false) == "minecraft:block.fence_gate.close");
    // parity: `BlockSetType.IRON` gives an iron door its own sound even though a hand cannot move it.
    CHECK(sound("iron_door", true) == "minecraft:block.iron_door.open");
    CHECK(sound("iron_trapdoor", false) == "minecraft:block.iron_trapdoor.close");
    CHECK(sound("bamboo_door", true) == "minecraft:block.bamboo_wood_door.open");
    CHECK(sound("bamboo_trapdoor", true) == "minecraft:block.bamboo_wood_trapdoor.open");
    CHECK(sound("bamboo_fence_gate", true) == "minecraft:block.bamboo_wood_fence_gate.open");
    CHECK(sound("cherry_door", false) == "minecraft:block.cherry_wood_door.close");
    CHECK(sound("crimson_fence_gate", true) == "minecraft:block.nether_wood_fence_gate.open");
    CHECK(sound("warped_trapdoor", false) == "minecraft:block.nether_wood_trapdoor.close");
    CHECK(sound("spruce_door", true) == "minecraft:block.wooden_door.open");

    // parity: every weathering stage shares `BlockSetType.COPPER`, so they all sound the same.
    CHECK(sound("copper_door", true) == "minecraft:block.copper_door.open");
    CHECK(sound("exposed_copper_door", true) == "minecraft:block.copper_door.open");
    CHECK(sound("weathered_copper_trapdoor", false) == "minecraft:block.copper_trapdoor.close");
    CHECK(sound("oxidized_copper_door", true) == "minecraft:block.copper_door.open");
    CHECK(sound("waxed_copper_door", false) == "minecraft:block.copper_door.close");
    CHECK(sound("waxed_oxidized_copper_trapdoor", true) == "minecraft:block.copper_trapdoor.open");

    CHECK_FALSE(sound("lever", true).has_value());
    CHECK_FALSE(sound("oak_button", true).has_value());
    CHECK_FALSE(sound("stone", true).has_value());
    CHECK_FALSE(sound("air", false).has_value());
}

TEST_CASE("a click turns a copper golem statue one pose on, and wraps at the last") {
    // parity: `CopperGolemStatueBlock.useItemOn` answers SUCCESS to anything that is not an axe, an
    // empty hand included, and `updatePose` writes `POSE.getNextPose()` with flag three. The next
    // pose wraps: `getNextPose` is a `ByIdMap.continuous` with `ZERO` out of bounds. What makes this
    // redstone and not decoration is that the pose is what a comparator beside it reads, one to four.
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const auto poseAt = [&world] { return propertyAt(world, 8, 64, 8, "copper_golem_pose"); };
    const auto put = [&world](const std::string& pose) {
        REQUIRE(
            world.setBlockState(8, 64, 8,
                                stateOf("copper_golem_statue",
                                        {{"copper_golem_pose", pose}, {"facing", "north"}, {"waterlogged", "false"}})));
    };

    put("standing");
    CHECK(blockInteractions::isCycleable(*world.blockState(8, 64, 8)));
    REQUIRE(blockInteractions::cycle(world, 8, 64, 8).has_value());
    CHECK(poseAt() == "sitting");
    REQUIRE(blockInteractions::cycle(world, 8, 64, 8).has_value());
    CHECK(poseAt() == "running");
    REQUIRE(blockInteractions::cycle(world, 8, 64, 8).has_value());
    CHECK(poseAt() == "star");
    // The wrap, which is the half a plain `next` would get wrong.
    REQUIRE(blockInteractions::cycle(world, 8, 64, 8).has_value());
    CHECK(poseAt() == "standing");
}

TEST_CASE("an axe scrapes a statue instead of turning it, and turns nothing else away") {
    // parity: `CopperGolemStatueBlock.useItemOn` returns PASS for `ItemTags.AXES` and SUCCESS for
    // everything else, an empty hand included. Ours is the only cycleable block that looks at the
    // hand, and the axe branch of the use handler sits **after** the cycle branch, so without this
    // an axe would turn the pose instead of scraping the copper.
    CHECK(blockInteractions::cycleYieldsToAxe(stateOf("copper_golem_statue")));
    CHECK_FALSE(blockInteractions::cycleYieldsToAxe(stateOf("repeater")));
    CHECK_FALSE(blockInteractions::cycleYieldsToAxe(stateOf("comparator")));
    CHECK_FALSE(blockInteractions::cycleYieldsToAxe(stateOf("daylight_detector")));
    CHECK_FALSE(blockInteractions::cycleYieldsToAxe(stateOf("redstone_wire")));
    CHECK_FALSE(blockInteractions::cycleYieldsToAxe(stateOf("stone")));
}
