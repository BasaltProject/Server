#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Reshapes.hpp>

#include <doctest.h>

#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::server::world::Chunk;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t stateOf(const std::string& path, const std::map<std::string, std::string>& properties) {
    const std::optional<std::int32_t> stateId = registry::blockStateDefinitions::stateIdOf(
        basalt::wire::common::Identifier::vanilla(path), properties, std::nullopt);
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] std::string propertyOf(std::int32_t stateId, const std::string& name) {
    const std::map<std::string, std::string> properties = registry::blockStateDefinitions::propertiesOf(stateId);
    const auto found = properties.find(name);
    return found == properties.end() ? std::string{} : found->second;
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// A world with the middle chunk in memory, since `WorldState` cannot be moved out of a helper.
struct Loaded {
    WorldState state = emptyWorld();

    Loaded() { state.getOrGenerateChunk(0, 0); }
};

/// Puts the block down and asks what its own shape makes of it.
[[nodiscard]] std::optional<std::int32_t> reshape(WorldState& world, std::int32_t x, std::int32_t y, std::int32_t z,
                                                  std::int32_t stateId, const reshapes::GeyserReshape& geyser = {}) {
    REQUIRE(world.setBlockState(x, y, z, stateId));
    return reshapes::of(world, x, y, z, stateId, geyser);
}

} // namespace

TEST_CASE("a block with no shape of its own is left alone") {
    Loaded loaded;
    WorldState& world = loaded.state;
    CHECK_FALSE(reshape(world, 8, 64, 8, stateOf("stone")).has_value());
    CHECK_FALSE(reshape(world, 8, 64, 8, stateOf("oak_planks")).has_value());
    CHECK_FALSE(reshapes::of(world, 8, 400, 8, air()).has_value());
}

TEST_CASE("a fence gate sinks into the wall beside it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t gate =
        stateOf("oak_fence_gate", {{"facing", "north"}, {"in_wall", "false"}, {"open", "false"}, {"powered", "false"}});

    CHECK_FALSE(reshape(world, 8, 64, 8, gate).has_value());

    // The gate stands across its facing, so the wall it sinks into is east or west of it.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("cobblestone_wall")));
    const std::optional<std::int32_t> sunk = reshape(world, 8, 64, 8, gate);
    REQUIRE(sunk.has_value());
    CHECK(propertyOf(*sunk, "in_wall") == "true");

    // A wall in front of it is not the one it sinks into.
    Loaded second;
    WorldState& other = second.state;
    REQUIRE(other.setBlockState(8, 64, 7, stateOf("cobblestone_wall")));
    CHECK_FALSE(reshape(other, 8, 64, 8, gate).has_value());
}

TEST_CASE("half a bed with no other half goes with it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t head = stateOf("red_bed", {{"facing", "north"}, {"part", "head"}, {"occupied", "false"}});

    const std::optional<std::int32_t> gone = reshape(world, 8, 64, 8, head);
    REQUIRE(gone.has_value());
    CHECK(*gone == air());
    CHECK(world.blockState(8, 64, 8) == air());
}

TEST_CASE("a bed half copies whether the other one is occupied") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t head = stateOf("red_bed", {{"facing", "north"}, {"part", "head"}, {"occupied", "false"}});
    const std::int32_t foot = stateOf("red_bed", {{"facing", "north"}, {"part", "foot"}, {"occupied", "true"}});
    // The head faces north, so its foot is the block to the south.
    REQUIRE(world.setBlockState(8, 64, 9, foot));

    const std::optional<std::int32_t> copied = reshape(world, 8, 64, 8, head);
    REQUIRE(copied.has_value());
    CHECK(propertyOf(*copied, "occupied") == "true");
    CHECK(propertyOf(*copied, "part") == "head");
}

TEST_CASE("a door half follows the other half and dies without it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::map<std::string, std::string> shared{
        {"facing", "north"}, {"hinge", "left"}, {"open", "true"}, {"powered", "false"}};
    std::map<std::string, std::string> upperProperties = shared;
    upperProperties.emplace("half", "upper");
    std::map<std::string, std::string> lowerProperties = shared;
    lowerProperties.emplace("half", "lower");
    lowerProperties["open"] = "false";
    const std::int32_t upper = stateOf("oak_door", upperProperties);
    const std::int32_t lower = stateOf("oak_door", lowerProperties);
    REQUIRE(world.setBlockState(8, 65, 8, upper));

    const std::optional<std::int32_t> followed = reshape(world, 8, 64, 8, lower);
    REQUIRE(followed.has_value());
    CHECK(propertyOf(*followed, "half") == "lower");
    CHECK(propertyOf(*followed, "open") == "true");

    REQUIRE(world.setBlockState(8, 65, 8, air()));
    const std::optional<std::int32_t> gone = reshape(world, 8, 64, 8, lower);
    REQUIRE(gone.has_value());
    CHECK(*gone == air());
}

TEST_CASE("a gate laid the other way sinks into the walls on its other axis") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t gate =
        stateOf("oak_fence_gate", {{"facing", "east"}, {"in_wall", "false"}, {"open", "false"}, {"powered", "false"}});

    // Facing east, the walls it looks for are north and south of it.
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("cobblestone_wall")));
    CHECK_FALSE(reshape(world, 8, 64, 8, gate).has_value());

    REQUIRE(world.setBlockState(8, 64, 9, stateOf("cobblestone_wall")));
    const std::optional<std::int32_t> sunk = reshape(world, 8, 64, 8, gate);
    REQUIRE(sunk.has_value());
    CHECK(propertyOf(*sunk, "in_wall") == "true");
}

TEST_CASE("a bed finds its other half whichever way it lies") {
    const auto lyingAlong = [](const std::string& facing, std::int32_t dx, std::int32_t dz) {
        Loaded loaded;
        WorldState& world = loaded.state;
        const std::int32_t head = stateOf("red_bed", {{"facing", facing}, {"part", "head"}, {"occupied", "false"}});
        const std::int32_t foot = stateOf("red_bed", {{"facing", facing}, {"part", "foot"}, {"occupied", "true"}});
        REQUIRE(world.setBlockState(8 + dx, 64, 8 + dz, foot));
        const std::optional<std::int32_t> copied = reshape(world, 8, 64, 8, head);
        return copied.has_value() && propertyOf(*copied, "occupied") == "true";
    };

    // The head faces one way and its foot is the block behind it.
    CHECK(lyingAlong("north", 0, 1));
    CHECK(lyingAlong("south", 0, -1));
    CHECK(lyingAlong("west", 1, 0));
    CHECK(lyingAlong("east", -1, 0));
}

TEST_CASE("the upper half of a door follows the lower one") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::map<std::string, std::string> shared{{"facing", "north"}, {"hinge", "left"}, {"powered", "false"}};
    std::map<std::string, std::string> lowerProperties = shared;
    lowerProperties.emplace("half", "lower");
    lowerProperties.emplace("open", "true");
    std::map<std::string, std::string> upperProperties = shared;
    upperProperties.emplace("half", "upper");
    upperProperties.emplace("open", "false");
    REQUIRE(world.setBlockState(8, 64, 8, stateOf("oak_door", lowerProperties)));

    const std::optional<std::int32_t> followed = reshape(world, 8, 65, 8, stateOf("oak_door", upperProperties));
    REQUIRE(followed.has_value());
    CHECK(propertyOf(*followed, "half") == "upper");
    CHECK(propertyOf(*followed, "open") == "true");
}

TEST_CASE("a chest takes the side the chest next to it left free") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto chest = [](const std::string& type) {
        return stateOf("chest", {{"facing", "north"}, {"type", type}, {"waterlogged", "false"}});
    };
    // A chest facing north has its left to the west.
    REQUIRE(world.setBlockState(7, 64, 8, chest("left")));

    const std::optional<std::int32_t> paired = reshape(world, 8, 64, 8, chest("single"));
    REQUIRE(paired.has_value());
    CHECK(propertyOf(*paired, "type") == "right");

    // And it goes back to single when that chest is gone.
    REQUIRE(world.setBlockState(7, 64, 8, air()));
    const std::optional<std::int32_t> alone = reshape(world, 8, 64, 8, chest("right"));
    REQUIRE(alone.has_value());
    CHECK(propertyOf(*alone, "type") == "single");
}

TEST_CASE("a chest facing another way is not a partner") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(7, 64, 8,
                                stateOf("chest", {{"facing", "south"}, {"type", "left"}, {"waterlogged", "false"}})));

    CHECK_FALSE(
        reshape(world, 8, 64, 8, stateOf("chest", {{"facing", "north"}, {"type", "single"}, {"waterlogged", "false"}}))
            .has_value());
}

TEST_CASE("a chest that claimed the wrong half is not a partner") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto chest = [](const std::string& type) {
        return stateOf("chest", {{"facing", "north"}, {"type", type}, {"waterlogged", "false"}});
    };
    // A left chest is to the west, so another left chest there is not the other half of anything.
    REQUIRE(world.setBlockState(7, 64, 8, chest("right")));
    CHECK_FALSE(reshape(world, 8, 64, 8, chest("single")).has_value());

    // A left chest looks for its partner clockwise from its facing, which is the other side.
    REQUIRE(world.setBlockState(7, 64, 8, air()));
    REQUIRE(world.setBlockState(9, 64, 8, chest("right")));
    CHECK_FALSE(reshape(world, 8, 64, 8, chest("left")).has_value());
    REQUIRE(world.setBlockState(9, 64, 8,
                                stateOf("chest", {{"facing", "south"}, {"type", "right"}, {"waterlogged", "false"}})));
    const std::optional<std::int32_t> alone = reshape(world, 8, 64, 8, chest("left"));
    REQUIRE(alone.has_value());
    CHECK(propertyOf(*alone, "type") == "single");
}

TEST_CASE("every copper chest pairs like the wooden one") {
    CHECK(reshapes::chestBlocks().size() == 10);
    for (const std::string_view name : reshapes::chestBlocks()) {
        Loaded loaded;
        WorldState& world = loaded.state;
        const std::string path{name};
        REQUIRE(world.setBlockState(7, 64, 8,
                                    stateOf(path, {{"facing", "north"}, {"type", "left"}, {"waterlogged", "false"}})));
        const std::optional<std::int32_t> paired = reshape(
            world, 8, 64, 8, stateOf(path, {{"facing", "north"}, {"type", "single"}, {"waterlogged", "false"}}));
        REQUIRE(paired.has_value());
        CHECK(propertyOf(*paired, "type") == "right");
    }
}

TEST_CASE("a chorus plant reaches for its own kind and for the end stone under it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(8, 63, 8, stateOf("end_stone")));
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("chorus_flower")));

    const std::optional<std::int32_t> plant = reshape(world, 8, 64, 8,
                                                      stateOf("chorus_plant", {{"down", "false"},
                                                                               {"east", "false"},
                                                                               {"north", "false"},
                                                                               {"south", "false"},
                                                                               {"up", "false"},
                                                                               {"west", "false"}}));

    REQUIRE(plant.has_value());
    CHECK(propertyOf(*plant, "down") == "true");
    CHECK(propertyOf(*plant, "east") == "true");
    CHECK(propertyOf(*plant, "up") == "false");
    CHECK(propertyOf(*plant, "north") == "false");
}

TEST_CASE("end stone anywhere but underneath does not hold a chorus plant") {
    Loaded loaded;
    WorldState& world = loaded.state;
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("end_stone")));

    CHECK_FALSE(reshape(world, 8, 64, 8,
                        stateOf("chorus_plant", {{"down", "false"},
                                                 {"east", "false"},
                                                 {"north", "false"},
                                                 {"south", "false"},
                                                 {"up", "false"},
                                                 {"west", "false"}}))
                    .has_value());
}

TEST_CASE("a campfire over hay is a signal fire") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t campfire =
        stateOf("campfire", {{"facing", "north"}, {"lit", "true"}, {"signal_fire", "false"}, {"waterlogged", "false"}});

    CHECK_FALSE(reshape(world, 8, 64, 8, campfire).has_value());

    REQUIRE(world.setBlockState(8, 63, 8, stateOf("hay_block")));
    const std::optional<std::int32_t> signal = reshape(world, 8, 64, 8, campfire);
    REQUIRE(signal.has_value());
    CHECK(propertyOf(*signal, "signal_fire") == "true");
}

TEST_CASE("the strand of hanging moss with nothing under it is the tip") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t moss = stateOf("pale_hanging_moss", {{"tip", "false"}});

    const std::optional<std::int32_t> tip = reshape(world, 8, 64, 8, moss);
    REQUIRE(tip.has_value());
    CHECK(propertyOf(*tip, "tip") == "true");

    REQUIRE(world.setBlockState(8, 63, 8, moss));
    CHECK_FALSE(reshape(world, 8, 64, 8, moss).has_value());
}

TEST_CASE("a dripstone takes the thickness the piece in front of it leaves") {
    const auto spike = [](const std::string& direction, const std::string& thickness) {
        return stateOf("pointed_dripstone",
                       {{"vertical_direction", direction}, {"thickness", thickness}, {"waterlogged", "false"}});
    };
    const auto thicknessWith = [&](std::int32_t below, std::int32_t above) {
        Loaded loaded;
        WorldState& world = loaded.state;
        if (below != air()) {
            REQUIRE(world.setBlockState(8, 63, 8, below));
        }
        if (above != air()) {
            REQUIRE(world.setBlockState(8, 65, 8, above));
        }
        const std::optional<std::int32_t> shaped = reshape(world, 8, 64, 8, spike("down", "middle"));
        return shaped.has_value() ? propertyOf(*shaped, "thickness") : std::string{"middle"};
    };

    // Nothing under it at all: it is the point.
    CHECK(thicknessWith(air(), air()) == "tip");
    // Another spike pointing the same way under it, and none behind: it is the base.
    CHECK(thicknessWith(spike("down", "tip"), air()) == "frustum");
    CHECK(thicknessWith(spike("down", "middle"), air()) == "base");
    CHECK(thicknessWith(spike("down", "middle"), spike("down", "middle")) == "middle");
    // A spike pointing back at it head on is a tip until one of the two has already merged, which
    // is the pair that catches a drop.
    CHECK(thicknessWith(spike("up", "tip"), air()) == "tip");
    CHECK(thicknessWith(spike("up", "tip_merge"), air()) == "tip_merge");
}

TEST_CASE("a stalagmite reads the piece above it, not the one below") {
    const auto spike = [](const std::string& direction, const std::string& thickness) {
        return stateOf("pointed_dripstone",
                       {{"vertical_direction", direction}, {"thickness", thickness}, {"waterlogged", "false"}});
    };
    const auto thicknessWith = [&](std::int32_t below, std::int32_t above) {
        Loaded loaded;
        WorldState& world = loaded.state;
        if (below != air()) {
            REQUIRE(world.setBlockState(8, 63, 8, below));
        }
        if (above != air()) {
            REQUIRE(world.setBlockState(8, 65, 8, above));
        }
        const std::optional<std::int32_t> shaped = reshape(world, 8, 64, 8, spike("up", "middle"));
        return shaped.has_value() ? propertyOf(*shaped, "thickness") : std::string{"middle"};
    };

    // One that points up grows the other way, so its front is above and its back below.
    CHECK(thicknessWith(air(), air()) == "tip");
    CHECK(thicknessWith(air(), spike("up", "tip")) == "frustum");
    CHECK(thicknessWith(air(), spike("up", "middle")) == "base");
    CHECK(thicknessWith(spike("up", "middle"), spike("up", "middle")) == "middle");
}

TEST_CASE("a mushroom cap stops drawing the faces it shares with another") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto cap = [](const std::string& value) {
        return stateOf(
            "red_mushroom_block",
            {{"down", value}, {"east", value}, {"north", value}, {"south", value}, {"up", value}, {"west", value}});
    };
    REQUIRE(world.setBlockState(9, 64, 8, cap("true")));

    const std::optional<std::int32_t> shaped = reshape(world, 8, 64, 8, cap("true"));
    REQUIRE(shaped.has_value());
    CHECK(propertyOf(*shaped, "east") == "false");
    CHECK(propertyOf(*shaped, "west") == "true");
    CHECK(propertyOf(*shaped, "up") == "true");
}

TEST_CASE("a bell with a wall on both sides hangs from both") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto bell = [](const std::string& attachment) {
        return stateOf("bell", {{"attachment", attachment}, {"facing", "north"}, {"powered", "false"}});
    };
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));

    // One wall only, which is what it already says.
    CHECK_FALSE(reshape(world, 8, 64, 8, bell("single_wall")).has_value());

    REQUIRE(world.setBlockState(8, 64, 9, stateOf("stone")));
    const std::optional<std::int32_t> doubled = reshape(world, 8, 64, 8, bell("single_wall"));
    REQUIRE(doubled.has_value());
    CHECK(propertyOf(*doubled, "attachment") == "double_wall");

    // A bell on the floor or the ceiling is none of this.
    CHECK_FALSE(reshape(world, 8, 64, 8, bell("floor")).has_value());
    CHECK_FALSE(reshape(world, 8, 64, 8, bell("ceiling")).has_value());
}

TEST_CASE("picking the melon frees the stem that fed it") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t attached = stateOf("attached_melon_stem", {{"facing", "east"}});
    REQUIRE(world.setBlockState(9, 64, 8, stateOf("melon")));

    CHECK_FALSE(reshape(world, 8, 64, 8, attached).has_value());

    REQUIRE(world.setBlockState(9, 64, 8, air()));
    const std::optional<std::int32_t> freed = reshape(world, 8, 64, 8, attached);
    REQUIRE(freed.has_value());
    CHECK(*freed == stateOf("melon_stem", {{"age", "7"}}));

    // And the pumpkin stem is its own block, not the melon one under another name.
    Loaded pumpkinWorld;
    WorldState& pumpkins = pumpkinWorld.state;
    const std::optional<std::int32_t> pumpkinFreed =
        reshape(pumpkins, 8, 64, 8, stateOf("attached_pumpkin_stem", {{"facing", "east"}}));
    REQUIRE(pumpkinFreed.has_value());
    CHECK(*pumpkinFreed == stateOf("pumpkin_stem", {{"age", "7"}}));
}

TEST_CASE("a moss carpet keeps the sides that have a wall to hold them") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto carpet = [](const std::string& bottom, const std::string& north) {
        return stateOf("pale_moss_carpet",
                       {{"bottom", bottom}, {"north", north}, {"south", "none"}, {"west", "none"}, {"east", "none"}});
    };
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));

    // The wall is there, so the side stays and the carpet with it.
    CHECK_FALSE(reshape(world, 8, 64, 8, carpet("false", "low")).has_value());

    // Take the wall away and it has nothing left to stand on.
    REQUIRE(world.setBlockState(8, 64, 7, air()));
    const std::optional<std::int32_t> gone = reshape(world, 8, 64, 8, carpet("false", "low"));
    REQUIRE(gone.has_value());
    CHECK(*gone == air());
}

TEST_CASE("a moss carpet grows tall under another carpet's side") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto carpet = [](const std::string& bottom, const std::string& north) {
        return stateOf("pale_moss_carpet",
                       {{"bottom", bottom}, {"north", north}, {"south", "none"}, {"west", "none"}, {"east", "none"}});
    };
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 65, 8, carpet("false", "low")));

    const std::optional<std::int32_t> tall = reshape(world, 8, 64, 8, carpet("true", "low"));
    REQUIRE(tall.has_value());
    CHECK(propertyOf(*tall, "north") == "tall");
}

TEST_CASE("a moss carpet drops the side the carpet under it has already dropped") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const auto carpet = [](const std::string& bottom, const std::string& north) {
        return stateOf("pale_moss_carpet",
                       {{"bottom", bottom}, {"north", north}, {"south", "none"}, {"west", "none"}, {"east", "none"}});
    };
    REQUIRE(world.setBlockState(8, 64, 7, stateOf("stone")));
    REQUIRE(world.setBlockState(8, 63, 8, carpet("true", "none")));

    // The wall is there, but the carpet below has no side of its own on it, so this one lets go.
    const std::optional<std::int32_t> gone = reshape(world, 8, 64, 8, carpet("false", "low"));
    REQUIRE(gone.has_value());
    CHECK(*gone == air());
}

TEST_CASE("a geyser answers for itself") {
    Loaded loaded;
    WorldState& world = loaded.state;
    const std::int32_t sulfur = stateOf("potent_sulfur");
    std::int32_t asked = 0;
    const reshapes::GeyserReshape geyser = [&](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
        CHECK(x == 8);
        CHECK(y == 64);
        CHECK(z == 8);
        CHECK(stateId == sulfur);
        ++asked;
        return std::optional{stateId};
    };

    CHECK(reshape(world, 8, 64, 8, sulfur, geyser) == sulfur);
    CHECK(asked == 1);

    // With nobody to ask, it stays as it is.
    CHECK_FALSE(reshape(world, 8, 64, 8, sulfur).has_value());
    CHECK(asked == 1);
}
