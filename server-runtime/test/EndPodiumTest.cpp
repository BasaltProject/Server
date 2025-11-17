#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/EndPodium.hpp>

#include <doctest.h>

#include <string>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
using basalt::server::world::Chunk;
using basalt::server::world::ChunkHeight;
using basalt::server::world::kEndHeight;
using basalt::wire::common::Identifier;

namespace {

/// How high the terrain of the test world stands, so the podium has a surface to sit on.
constexpr std::int32_t kSurfaceY = 63;

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t torchState(const std::string& facing) {
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(Identifier::vanilla("wall_torch"), {{"facing", facing}});
    REQUIRE(stateId.has_value());
    return *stateId;
}

/// An End-shaped world whose chunks arrive empty.
[[nodiscard]] WorldState voidWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ, kEndHeight}; }};
}

/// The same, with stone up to the surface: a block the podium never places, so every block it does
/// place can be told apart from the ground it replaced.
[[nodiscard]] WorldState stoneWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) {
                          Chunk chunk{chunkX, chunkZ, kEndHeight};
                          const std::int32_t stone = registry::blocks::stone().stateId;
                          for (std::int32_t x = 0; x < 16; ++x) {
                              for (std::int32_t z = 0; z < 16; ++z) {
                                  for (std::int32_t y = 0; y <= kSurfaceY; ++y) {
                                      chunk.setBlockStateId(x, y, z, stone);
                                  }
                              }
                          }
                          return chunk;
                      }};
}

} // namespace

TEST_CASE("an empty end puts the podium at the bottom of the world") {
    WorldState world = voidWorld();
    CHECK_FALSE(endPodium::exists(world));

    const std::optional<std::array<std::int32_t, 3>> built = endPodium::ensure(world, false);
    REQUIRE(built.has_value());
    // Nothing to stand on, so the origin is the clamp: one block off the floor.
    CHECK(*built == std::array<std::int32_t, 3>{0, kEndHeight.minY() + 1, 0});
    CHECK(world.loadedChunkCount() == 9);
    CHECK(endPodium::exists(world));
    CHECK_FALSE(endPodium::ensure(world, false).has_value());
}

TEST_CASE("the podium sits on the surface it finds") {
    WorldState world = stoneWorld();
    const std::optional<std::array<std::int32_t, 3>> built = endPodium::ensure(world, false);
    REQUIRE(built.has_value());
    CHECK(*built == std::array<std::int32_t, 3>{0, kSurfaceY, 0});
}

TEST_CASE("bedrock under the origin is dug through, but never past the lowest origin") {
    WorldState world = voidWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t stone = registry::blocks::stone().stateId;
    for (std::int32_t y = 0; y < 60; ++y) {
        REQUIRE(world.setBlockState(0, y, 0, stone));
    }
    for (std::int32_t y = 60; y <= 70; ++y) {
        REQUIRE(world.setBlockState(0, y, 0, registry::blocks::bedrock().stateId));
    }

    // Bedrock anywhere in the column is a podium as far as `exists` is concerned, so the search is
    // reached by giving it a height that stops under this one.
    const ChunkHeight shallow{0, 48};
    REQUIRE_FALSE(endPodium::exists(world, shallow));
    const std::optional<std::array<std::int32_t, 3>> built = endPodium::ensure(world, false, shallow);
    REQUIRE(built.has_value());
    // Down from the surface at 71 through seven blocks of bedrock, and then it stops rather than
    // digging out the last one.
    CHECK(*built == std::array<std::int32_t, 3>{0, 63, 0});
}

TEST_CASE("the shape of a closed podium") {
    WorldState world = stoneWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t stone = registry::blocks::stone().stateId;
    REQUIRE(world.setBlockState(1, kSurfaceY + 32, 0, stone));
    REQUIRE(world.setBlockState(1, kSurfaceY + 33, 0, stone));

    const std::optional<std::array<std::int32_t, 3>> built = endPodium::ensure(world, false);
    REQUIRE(built.has_value());
    const std::int32_t origin = kSurfaceY;

    const std::int32_t bedrock = registry::blocks::bedrock().stateId;
    const std::int32_t air = registry::blocks::air().stateId;
    const std::int32_t endStone = stateOf("end_stone");

    // Under the top: bedrock inside the rim, end stone on the ring around it.
    CHECK(world.blockState(0, origin - 1, 0) == bedrock);
    CHECK(world.blockState(2, origin - 1, 1) == bedrock);
    CHECK(world.blockState(2, origin - 1, 2) == endStone);
    CHECK(world.blockState(3, origin - 1, 1) == endStone);
    CHECK(world.blockState(0, origin - 1, 3) == endStone);
    // Past the outer radius nothing is touched, and neither is the block below the podium.
    CHECK(world.blockState(3, origin - 1, 2) == stone);
    CHECK(world.blockState(4, origin - 1, 0) == stone);
    CHECK(world.blockState(0, origin - 2, 0) == stone);

    // The top itself: open inside the rim, walled outside it, with the pillar in the middle.
    CHECK(world.blockState(1, origin, 0) == air);
    CHECK(world.blockState(2, origin, 1) == air);
    CHECK(world.blockState(3, origin, 1) == bedrock);
    CHECK(world.blockState(0, origin, 0) == bedrock);
    CHECK(world.blockState(0, origin + 1, 0) == bedrock);
    CHECK(world.blockState(0, origin + 2, 0) == bedrock);
    CHECK(world.blockState(0, origin + 3, 0) == bedrock);
    CHECK(world.blockState(0, origin + 4, 0) == air);

    // The four torches, each looking out of the side of the pillar it hangs on.
    CHECK(world.blockState(0, origin + 2, -1) == torchState("north"));
    CHECK(world.blockState(0, origin + 2, 1) == torchState("south"));
    CHECK(world.blockState(-1, origin + 2, 0) == torchState("west"));
    CHECK(world.blockState(1, origin + 2, 0) == torchState("east"));

    // The clearing stops exactly at the ceiling.
    CHECK(world.blockState(1, origin + 32, 0) == air);
    CHECK(world.blockState(1, origin + 33, 0) == stone);
}

TEST_CASE("an open podium has the portal in its middle") {
    WorldState world = stoneWorld();
    const std::optional<std::array<std::int32_t, 3>> built = endPodium::ensure(world, true);
    REQUIRE(built.has_value());
    const std::int32_t origin = kSurfaceY;
    const std::int32_t portal = stateOf("end_portal");

    CHECK(world.blockState(1, origin, 0) == portal);
    CHECK(world.blockState(2, origin, 1) == portal);
    CHECK(world.blockState(-2, origin, -1) == portal);
    // The pillar and the wall are the same whether it is open or not.
    CHECK(world.blockState(0, origin, 0) == registry::blocks::bedrock().stateId);
    CHECK(world.blockState(3, origin, 1) == registry::blocks::bedrock().stateId);
    CHECK(world.blockState(0, origin - 1, 0) == registry::blocks::bedrock().stateId);
}

TEST_CASE("only the column at the origin says the podium is there") {
    WorldState world = voidWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t bedrock = registry::blocks::bedrock().stateId;
    REQUIRE(world.setBlockState(1, 10, 0, bedrock));
    REQUIRE(world.setBlockState(0, 10, 1, bedrock));
    CHECK_FALSE(endPodium::exists(world));

    REQUIRE(world.setBlockState(0, 10, 0, bedrock));
    CHECK(endPodium::exists(world));
}

TEST_CASE("the search runs the height it is given and no further") {
    WorldState world = voidWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(0, 200, 0, registry::blocks::bedrock().stateId));
    CHECK(endPodium::exists(world));
    CHECK_FALSE(endPodium::exists(world, ChunkHeight{0, 128}));

    // Both ends of the range count.
    WorldState floor = voidWorld();
    floor.getOrGenerateChunk(0, 0);
    REQUIRE(floor.setBlockState(0, kEndHeight.minY(), 0, registry::blocks::bedrock().stateId));
    CHECK(endPodium::exists(floor));

    WorldState roof = voidWorld();
    roof.getOrGenerateChunk(0, 0);
    REQUIRE(roof.setBlockState(0, kEndHeight.maxY(), 0, registry::blocks::bedrock().stateId));
    CHECK(endPodium::exists(roof));
}
