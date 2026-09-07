#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/runtime/world/NaturalSpawnPredicates.hpp>

#include <doctest.h>

#include <cstdint>

using namespace basalt::server::runtime::world;

namespace {

constexpr std::int32_t kSeaLevel = 63;

[[nodiscard]] bool allows(std::string_view type, std::string_view ground, bool leaves = false,
                          std::string_view biome = "plains", std::int32_t y = kSeaLevel) {
    return naturalSpawnPredicates::allows(type, ground, leaves, biome, y, kSeaLevel);
}

} // namespace

TEST_CASE("three animals are placed without asking about the ground") {
    CHECK_FALSE(naturalSpawnPredicates::usesGroundPlacement("fox"));
    CHECK_FALSE(naturalSpawnPredicates::usesGroundPlacement("panda"));
    CHECK_FALSE(naturalSpawnPredicates::usesGroundPlacement("trader_llama"));
    CHECK(naturalSpawnPredicates::usesGroundPlacement("llama"));
    CHECK(naturalSpawnPredicates::usesGroundPlacement("cow"));
    // A type nobody named is placed on the ground like everything else.
    CHECK(naturalSpawnPredicates::usesGroundPlacement(""));
}

TEST_CASE("an animal with no rule of its own wants grass") {
    CHECK(allows("cow", "grass_block"));
    CHECK(allows("chicken", "grass_block"));
    CHECK_FALSE(allows("cow", "dirt"));
    CHECK_FALSE(allows("cow", "sand"));
    CHECK(allows("", "grass_block"));
}

TEST_CASE("the ground each animal will stand on") {
    CHECK(allows("armadillo", "red_sand"));
    CHECK(allows("armadillo", "light_gray_terracotta"));
    CHECK(allows("armadillo", "grass_block"));
    CHECK_FALSE(allows("armadillo", "sand"));
    CHECK_FALSE(allows("armadillo", "cyan_terracotta"));

    CHECK(allows("camel", "sand"));
    CHECK(allows("camel", "suspicious_sand"));
    CHECK_FALSE(allows("camel", "sandstone"));

    CHECK(allows("fox", "podzol"));
    CHECK(allows("wolf", "snow_block"));
    CHECK_FALSE(allows("wolf", "sand"));

    CHECK(allows("frog", "muddy_mangrove_roots"));
    CHECK_FALSE(allows("frog", "mangrove_log"));

    CHECK(allows("goat", "packed_ice"));
    CHECK(allows("goat", "gravel"));
    CHECK_FALSE(allows("goat", "ice"));

    CHECK(allows("mooshroom", "mycelium"));
    CHECK_FALSE(allows("mooshroom", "grass_block"));

    CHECK(allows("rabbit", "sand"));
    CHECK(allows("rabbit", "snow"));
    CHECK_FALSE(allows("rabbit", "red_sand"));
}

TEST_CASE("a parrot takes grass, air, any log or stem, or leaves") {
    CHECK(allows("parrot", "grass_block"));
    CHECK(allows("parrot", "air"));
    CHECK(allows("parrot", "oak_log"));
    CHECK(allows("parrot", "stripped_birch_wood"));
    CHECK(allows("parrot", "warped_hyphae"));
    CHECK(allows("parrot", "stripped_crimson_stem"));
    // Leaves are told apart by the caller, not by the name.
    CHECK(allows("parrot", "azalea_leaves", true));
    CHECK_FALSE(allows("parrot", "azalea_leaves", false));
    CHECK_FALSE(allows("parrot", "stone"));
    CHECK_FALSE(allows("parrot", "crimson_planks"));
}

TEST_CASE("a polar bear wants ice in the two frozen oceans and grass everywhere else") {
    CHECK(allows("polar_bear", "ice", false, "frozen_ocean"));
    CHECK(allows("polar_bear", "ice", false, "deep_frozen_ocean"));
    CHECK_FALSE(allows("polar_bear", "grass_block", false, "frozen_ocean"));
    CHECK(allows("polar_bear", "grass_block", false, "snowy_plains"));
    CHECK_FALSE(allows("polar_bear", "ice", false, "snowy_plains"));
    // A biome nobody named is not one of the two.
    CHECK(allows("polar_bear", "grass_block", false, ""));
}

TEST_CASE("a turtle stays under four blocks over the sea") {
    CHECK(allows("turtle", "sand", false, "beach", kSeaLevel + 3));
    CHECK_FALSE(allows("turtle", "sand", false, "beach", kSeaLevel + 4));
    CHECK(allows("turtle", "red_sand", false, "beach", kSeaLevel));
    CHECK_FALSE(allows("turtle", "gravel", false, "beach", kSeaLevel));
}

TEST_CASE("the four monsters that will not spawn away from the sky") {
    CHECK(naturalSpawnPredicates::isSurfaceMonster("husk"));
    CHECK(naturalSpawnPredicates::isSurfaceMonster("camel_husk"));
    CHECK(naturalSpawnPredicates::isSurfaceMonster("parched"));
    CHECK(naturalSpawnPredicates::isSurfaceMonster("stray"));
    CHECK_FALSE(naturalSpawnPredicates::isSurfaceMonster("zombie"));
    CHECK_FALSE(naturalSpawnPredicates::isSurfaceMonster(""));
}

TEST_CASE("water placement wants water here and nothing conducting over it") {
    CHECK(naturalSpawnPredicates::waterPlacementAllows(true, false));
    CHECK_FALSE(naturalSpawnPredicates::waterPlacementAllows(true, true));
    CHECK_FALSE(naturalSpawnPredicates::waterPlacementAllows(false, false));
    CHECK_FALSE(naturalSpawnPredicates::waterPlacementAllows(false, true));
}

TEST_CASE("a glow squid wants thirty-three blocks of water over it and no light at all") {
    CHECK(naturalSpawnPredicates::glowSquidAllows(30, 63, 0, true));
    CHECK(naturalSpawnPredicates::glowSquidAllows(63 - 33, 63, 0, true));
    CHECK_FALSE(naturalSpawnPredicates::glowSquidAllows(63 - 32, 63, 0, true));
    CHECK_FALSE(naturalSpawnPredicates::glowSquidAllows(30, 63, 1, true));
    CHECK_FALSE(naturalSpawnPredicates::glowSquidAllows(30, 63, 0, false));
}

TEST_CASE("a guardian only looks at the sky when its roll misses") {
    std::int32_t asked = 0;
    const auto sky = [&asked]() {
        ++asked;
        return true;
    };

    // The roll wins outright and the sky is never asked.
    CHECK(naturalSpawnPredicates::guardianAllows(0, "normal", true, true, sky));
    CHECK(asked == 0);

    // The roll misses, so an open sky refuses it.
    CHECK_FALSE(naturalSpawnPredicates::guardianAllows(1, "normal", true, true, sky));
    CHECK(asked == 1);

    const auto dark = []() { return false; };
    CHECK(naturalSpawnPredicates::guardianAllows(1, "normal", true, true, dark));
    CHECK_FALSE(naturalSpawnPredicates::guardianAllows(0, "peaceful", true, true, dark));
    CHECK_FALSE(naturalSpawnPredicates::guardianAllows(0, "normal", false, true, dark));
    CHECK_FALSE(naturalSpawnPredicates::guardianAllows(0, "normal", true, false, dark));
}
