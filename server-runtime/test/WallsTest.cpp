#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/Walls.hpp>

#include <doctest.h>

#include <map>
#include <string>

using namespace basalt::server::runtime::world;
using basalt::wire::common::Identifier;
namespace registry = basalt::server::registry;

namespace {

using Shape = std::map<std::string, std::string>;

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t air() { return registry::blocks::air().stateId; }

[[nodiscard]] std::int32_t wall() { return stateOf("cobblestone_wall"); }

/// A wall with its own post already up, which is what a wall above looks at.
[[nodiscard]] std::int32_t wallWithPost(const std::string& up) {
    const std::optional<std::int32_t> stateId =
        registry::blockStateDefinitions::stateIdOf(Identifier::vanilla("cobblestone_wall"), {{"up", up}}, wall());
    REQUIRE(stateId.has_value());
    return *stateId;
}

/// The shape of a wall with nothing around it but what is named.
[[nodiscard]] Shape shapeOf(std::int32_t north = 0, std::int32_t south = 0, std::int32_t west = 0,
                            std::int32_t east = 0, std::int32_t above = 0) {
    const std::optional<Shape> shape = walls::shapeOf(wall(), north, south, west, east, above);
    REQUIRE(shape.has_value());
    return *shape;
}

} // namespace

TEST_CASE("only a wall has a wall shape") {
    CHECK(walls::shapeOf(wall(), air(), air(), air(), air(), air()).has_value());
    CHECK(walls::shapeOf(stateOf("mossy_cobblestone_wall"), air(), air(), air(), air(), air()).has_value());
    CHECK_FALSE(walls::shapeOf(stateOf("oak_fence"), air(), air(), air(), air(), air()).has_value());
    CHECK_FALSE(walls::shapeOf(stateOf("stone"), air(), air(), air(), air(), air()).has_value());
    CHECK_FALSE(walls::shapeOf(air(), air(), air(), air(), air(), air()).has_value());
}

TEST_CASE("a wall alone reaches out to nothing and keeps its post") {
    const Shape shape = shapeOf(air(), air(), air(), air(), air());

    CHECK(shape.at("north") == "none");
    CHECK(shape.at("south") == "none");
    CHECK(shape.at("west") == "none");
    CHECK(shape.at("east") == "none");
    // parity: nothing on any side is a corner as far as the post is concerned.
    CHECK(shape.at("up") == "true");
}

TEST_CASE("each side reaches out on its own") {
    CHECK(shapeOf(stateOf("stone"), air(), air(), air(), air()).at("north") == "low");
    CHECK(shapeOf(air(), stateOf("stone"), air(), air(), air()).at("south") == "low");
    CHECK(shapeOf(air(), air(), stateOf("stone"), air(), air()).at("west") == "low");
    CHECK(shapeOf(air(), air(), air(), stateOf("stone"), air()).at("east") == "low");
    // And a side with nothing beside it stays none while the others reach out.
    CHECK(shapeOf(stateOf("stone"), air(), air(), air(), air()).at("south") == "none");
}

TEST_CASE("a wall reaches out to a wall, a pane and anything a fence would take") {
    CHECK(shapeOf(wall(), air(), air(), air(), air()).at("north") == "low");
    CHECK(shapeOf(stateOf("iron_bars"), air(), air(), air(), air()).at("north") == "low");
    CHECK(shapeOf(stateOf("glass_pane"), air(), air(), air(), air()).at("north") == "low");
    CHECK(shapeOf(stateOf("stone"), air(), air(), air(), air()).at("north") == "low");
    // parity: what a fence refuses, a wall refuses too.
    CHECK(shapeOf(stateOf("torch"), air(), air(), air(), air()).at("north") == "none");
    CHECK(shapeOf(stateOf("oak_leaves"), air(), air(), air(), air()).at("north") == "none");
}

TEST_CASE("a full block on top makes every side it covers grow tall") {
    const Shape under =
        shapeOf(stateOf("stone"), stateOf("stone"), stateOf("stone"), stateOf("stone"), stateOf("stone"));

    CHECK(under.at("north") == "tall");
    CHECK(under.at("south") == "tall");
    CHECK(under.at("west") == "tall");
    CHECK(under.at("east") == "tall");

    // With nothing on top the same four sides stay low.
    const Shape open = shapeOf(stateOf("stone"), stateOf("stone"), stateOf("stone"), stateOf("stone"), air());
    CHECK(open.at("north") == "low");
    CHECK(open.at("east") == "low");
}

TEST_CASE("a straight run of low sides loses its post, and a corner keeps it") {
    // North and south only: a straight line, nothing covering the middle.
    CHECK(shapeOf(stateOf("stone"), stateOf("stone"), air(), air(), air()).at("up") == "false");
    CHECK(shapeOf(air(), air(), stateOf("stone"), stateOf("stone"), air()).at("up") == "false");
    // A corner, and a T, both keep it.
    CHECK(shapeOf(stateOf("stone"), air(), stateOf("stone"), air(), air()).at("up") == "true");
    CHECK(shapeOf(stateOf("stone"), stateOf("stone"), stateOf("stone"), air(), air()).at("up") == "true");
    // parity: a crossroads is not a corner, so with nothing over the middle it loses the post too.
    CHECK(shapeOf(stateOf("stone"), stateOf("stone"), stateOf("stone"), stateOf("stone"), air()).at("up") == "false");
}

TEST_CASE("a straight run of tall sides loses its post whatever covers the middle") {
    // parity: `shouldRaisePost` answers false for a tall line before it looks at the middle at all.
    const Shape shape = shapeOf(stateOf("stone"), stateOf("stone"), air(), air(), stateOf("stone"));

    CHECK(shape.at("north") == "tall");
    CHECK(shape.at("south") == "tall");
    CHECK(shape.at("up") == "false");
}

TEST_CASE("a wall over a wall that has its own post keeps one too") {
    // parity: the first thing `shouldRaisePost` asks, before any of the side counting.
    CHECK(shapeOf(stateOf("stone"), stateOf("stone"), air(), air(), wallWithPost("true")).at("up") == "true");
    // A wall above without a post of its own settles nothing, so the straight run wins.
    CHECK(shapeOf(stateOf("stone"), stateOf("stone"), air(), air(), wallWithPost("false")).at("up") == "false");
}

TEST_CASE("a straight run under something that covers only the middle keeps its post") {
    // A torch covers the middle of the block above without covering the sides.
    const Shape shape = shapeOf(stateOf("stone"), stateOf("stone"), air(), air(), stateOf("cobblestone_slab"));

    CHECK(shape.at("north") == "tall");
    CHECK(shape.at("up") == "false");
}

TEST_CASE("something the post override tag names keeps the post up over a straight run") {
    // parity: `BlockTags.WALL_POST_OVERRIDE`, asked before the shape of the block above is.
    const std::optional<Shape> overridden =
        walls::shapeOf(wall(), stateOf("stone"), stateOf("stone"), air(), air(), stateOf("torch"), true);
    REQUIRE(overridden.has_value());
    CHECK(overridden->at("up") == "true");

    // The same torch without the tag settles nothing, because it covers none of the middle.
    const std::optional<Shape> plain =
        walls::shapeOf(wall(), stateOf("stone"), stateOf("stone"), air(), air(), stateOf("torch"), false);
    REQUIRE(plain.has_value());
    CHECK(plain->at("up") == "false");

    // And a tall run answers false before the tag is ever looked at.
    const std::optional<Shape> tall =
        walls::shapeOf(wall(), stateOf("stone"), stateOf("stone"), air(), air(), stateOf("stone"), true);
    REQUIRE(tall.has_value());
    CHECK(tall->at("north") == "tall");
    CHECK(tall->at("up") == "false");
}

TEST_CASE("a block that covers one side of the middle and not the other makes only that side tall") {
    // An extended piston reaches out one way, so what it covers over a wall is not symmetric.
    const std::optional<std::int32_t> pointed = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("sticky_piston"), {{"extended", "true"}, {"facing", "north"}}, std::nullopt);
    REQUIRE(pointed.has_value());

    const Shape shape = shapeOf(stateOf("stone"), stateOf("stone"), air(), air(), *pointed);

    // The two sides read different windows of the same block, so exactly one of them is tall.
    CHECK(shape.at("north") != shape.at("south"));
    // parity: one tall side is not a tall line, so the post is decided by the middle instead.
    CHECK(shape.at("up") == "true");

    // The same across the other axis, which the tall-line test asks about separately.
    const std::optional<std::int32_t> sideways = registry::blockStateDefinitions::stateIdOf(
        Identifier::vanilla("sticky_piston"), {{"extended", "true"}, {"facing", "east"}}, std::nullopt);
    REQUIRE(sideways.has_value());
    const Shape across = shapeOf(air(), air(), stateOf("stone"), stateOf("stone"), *sideways);
    CHECK(across.at("west") != across.at("east"));
    CHECK(across.at("up") == "true");

    // And a tall pair on that axis does lose the post.
    const Shape tall = shapeOf(air(), air(), stateOf("stone"), stateOf("stone"), stateOf("stone"));
    CHECK(tall.at("west") == "tall");
    CHECK(tall.at("east") == "tall");
    CHECK(tall.at("up") == "false");
}

TEST_CASE("the post looks at the middle two-sixteenths and no wider") {
    // Iron bars cover the middle column and nothing on either side of it, which is what tells a
    // window of 14 to 18 apart from one that starts a thirty-second earlier.
    const Shape shape = shapeOf(stateOf("stone"), stateOf("stone"), air(), air(), stateOf("iron_bars"));

    CHECK(shape.at("north") == "low");
    CHECK(shape.at("south") == "low");
    CHECK(shape.at("up") == "true");
}
