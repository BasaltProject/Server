#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/GrowthSupport.hpp>
#include <basalt/server/runtime/world/WorldLight.hpp>

#include <doctest.h>

#include <string>
#include <vector>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
using basalt::server::world::Chunk;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

} // namespace

TEST_CASE("open sky is full light, and a roof takes it away") {
    WorldState world = emptyWorld();
    for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            world.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    CHECK(worldLight::skyLight(world, 8, 64, 8) == worldLight::kMaxLevel);
    CHECK(worldLight::blockLight(world, 8, 64, 8) == 0);
    CHECK(worldLight::rawBrightness(world, 8, 64, 8) == worldLight::kMaxLevel);

    // A stone roof, and the block under it is dark: the light is worked out from the blocks, which
    // is the whole point of asking here rather than guessing.
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 70, z, stateOf("stone")));
        }
    }
    CHECK(worldLight::skyLight(world, 8, 64, 8) < worldLight::kMaxLevel);
    CHECK(worldLight::skyLight(world, 8, 71, 8) == worldLight::kMaxLevel);
}

TEST_CASE("light under the edge of a roof falls off one level per block") {
    WorldState world = emptyWorld();
    for (std::int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            world.getOrGenerateChunk(chunkX, chunkZ);
        }
    }
    // Half a roof: open sky from x = 8 eastwards, stone over everything west of it.
    for (std::int32_t x = 0; x < 8; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            REQUIRE(world.setBlockState(x, 70, z, stateOf("stone")));
        }
    }

    // Each block further in is one darker, which is what pins the exact value rather than a range:
    // reading the wrong half of a byte, or reading x where z belongs, answers with a neighbour.
    CHECK(worldLight::skyLight(world, 8, 64, 8) == 15);
    CHECK(worldLight::skyLight(world, 7, 64, 8) == 14);
    CHECK(worldLight::skyLight(world, 6, 64, 8) == 13);
    CHECK(worldLight::skyLight(world, 5, 64, 8) == 12);
    // Along z the roof does not change, so every z at the same x reads the same.
    CHECK(worldLight::skyLight(world, 7, 64, 7) == 14);
    CHECK(worldLight::skyLight(world, 7, 64, 9) == 14);
}

TEST_CASE("a world with no sky is dark whatever is above") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    CHECK(worldLight::skyLight(world, 8, 64, 8, true) == worldLight::kMaxLevel);
    // The Nether has no sky, so the same open column reads nothing at all.
    CHECK(worldLight::skyLight(world, 8, 64, 8, false) == 0);
    CHECK(worldLight::rawBrightness(world, 8, 64, 8, false) == 0);
}

TEST_CASE("the dampen a mob spawn asks for") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    // parity: `getRawBrightness(pos, skyDampen)` takes the dampen off the sky half only, which is
    // how a mob knows it is night without the sky light itself changing.
    CHECK(worldLight::rawBrightness(world, 8, 64, 8, true, 0) == 15);
    CHECK(worldLight::rawBrightness(world, 8, 64, 8, true, 11) == 4);
    CHECK(worldLight::rawBrightness(world, 8, 64, 8, true, 15) == 0);
}

TEST_CASE("light outside the world and outside a loaded chunk is nothing") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    CHECK(worldLight::skyLight(world, 500, 64, 500) == 0);
    CHECK(worldLight::skyLight(world, 8, 5000, 8) == 0);
    CHECK(worldLight::skyLight(world, 8, -5000, 8) == 0);
    CHECK(worldLight::blockLight(world, 8, 5000, 8) == 0);
}

TEST_CASE("the offsets every growing block shares") {
    CHECK(growthSupport::offsetX("west") == -1);
    CHECK(growthSupport::offsetX("east") == 1);
    CHECK(growthSupport::offsetX("north") == 0);
    CHECK(growthSupport::offsetY("down") == -1);
    CHECK(growthSupport::offsetY("up") == 1);
    CHECK(growthSupport::offsetY("north") == 0);
    CHECK(growthSupport::offsetZ("north") == -1);
    CHECK(growthSupport::offsetZ("south") == 1);
    CHECK(growthSupport::offsetZ("west") == 0);
    CHECK(growthSupport::offsetX("") == 0);

    CHECK(growthSupport::kHorizontalNames.size() == 4);
    CHECK(growthSupport::kFaceNames.size() == 6);
    // The light a crop needs, which is the same nine for every one of them.
    CHECK(growthSupport::kGrowthLight == 9);
}

TEST_CASE("a block written through the shared setter is recorded once") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    std::vector<BlockChange> changes;

    growthSupport::set(world, 1, 64, 1, stateOf("stone"), changes);
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].x == 1);
    CHECK(changes[0].y == 64);
    CHECK(changes[0].z == 1);
    CHECK(changes[0].stateId == stateOf("stone"));
    CHECK(world.blockState(1, 64, 1) == stateOf("stone"));

    // A position that is not loaded is not written and not reported.
    growthSupport::set(world, 500, 64, 500, stateOf("stone"), changes);
    CHECK(changes.size() == 1);
}

TEST_CASE("a crop ages one step at a time and stops at its last") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t young = stateOf("wheat");
    REQUIRE(world.setBlockState(2, 64, 2, young));
    CHECK(growthSupport::ageOf(young) == 0);

    std::vector<BlockChange> changes;
    for (std::int32_t age = 1; age <= 7; ++age) {
        CAPTURE(age);
        growthSupport::grow(world, 2, 64, 2, *world.blockState(2, 64, 2), 7, changes);
        CHECK(growthSupport::ageOf(*world.blockState(2, 64, 2)) == age);
    }
    CHECK(changes.size() == 7);

    // Fully grown: nothing more happens and nothing is reported.
    growthSupport::grow(world, 2, 64, 2, *world.blockState(2, 64, 2), 7, changes);
    CHECK(changes.size() == 7);

    // A cap below the block's own last age stops it there: a crop that stops early is one that
    // stops early, not one that grows to whatever its states allow.
    REQUIRE(world.setBlockState(4, 64, 4, young));
    for (std::int32_t at = 0; at < 5; ++at) {
        growthSupport::grow(world, 4, 64, 4, *world.blockState(4, 64, 4), 2, changes);
    }
    CHECK(growthSupport::ageOf(*world.blockState(4, 64, 4)) == 2);

    // A block with no age at all is left alone.
    REQUIRE(world.setBlockState(3, 64, 3, stateOf("stone")));
    growthSupport::grow(world, 3, 64, 3, stateOf("stone"), 7, changes);
    CHECK(changes.size() == 9);
    CHECK_FALSE(growthSupport::ageOf(stateOf("stone")).has_value());
}
