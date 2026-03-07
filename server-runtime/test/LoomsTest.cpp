#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/Looms.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime;
using basalt::server::storage::StoredItemStack;

TEST_CASE("a loom names its three inputs by what they are called") {
    CHECK(world::looms::dyeColourOf("minecraft:light_blue_dye") == "light_blue");
    CHECK_FALSE(world::looms::dyeColourOf("minecraft:iron_ingot").has_value());
    // parity: bone meal dyes, but it is not one of the sixteen the loom takes.
    CHECK_FALSE(world::looms::dyeColourOf("minecraft:bone_meal").has_value());

    CHECK(world::looms::patternTagOf("minecraft:flower_banner_pattern") == "minecraft:pattern_item/flower");
    CHECK_FALSE(world::looms::patternTagOf("minecraft:white_banner").has_value());

    CHECK(world::looms::isBanner("minecraft:white_banner"));
    CHECK_FALSE(world::looms::isBanner("minecraft:flower_banner_pattern"));
    CHECK_FALSE(world::looms::isBanner("minecraft:banner"));
}

TEST_CASE("weaving puts one more layer on and keeps the ones already there") {
    const StoredItemStack plain{.id = "minecraft:white_banner", .count = 3};
    const std::optional<StoredItemStack> once = world::looms::weave(plain, "minecraft:cross", "red");
    REQUIRE(once.has_value());
    // parity: the result is one banner however many went in.
    CHECK(once->count == 1);
    CHECK(world::looms::layersOn(*once) == 1);

    const std::optional<StoredItemStack> twice = world::looms::weave(*once, "minecraft:border", "blue");
    REQUIRE(twice.has_value());
    CHECK(world::looms::layersOn(*twice) == 2);
    CHECK(world::looms::layersOn(*once) == 1);
}

TEST_CASE("a banner already carrying six layers weaves nothing") {
    StoredItemStack banner{.id = "minecraft:white_banner", .count = 1};
    for (std::size_t layer = 0; layer < world::looms::kMaxLayers; ++layer) {
        const std::optional<StoredItemStack> made = world::looms::weave(banner, "minecraft:cross", "red");
        REQUIRE(made.has_value());
        banner = *made;
    }
    CHECK(world::looms::layersOn(banner) == world::looms::kMaxLayers);
    CHECK_FALSE(world::looms::weave(banner, "minecraft:border", "blue").has_value());
}
