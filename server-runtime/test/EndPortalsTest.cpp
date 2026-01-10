#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/runtime/world/EndPortals.hpp>

#include <doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

using namespace basalt::server::runtime::world;
namespace registry = basalt::server::registry;
using basalt::server::world::Chunk;
using basalt::wire::common::Identifier;

namespace {

[[nodiscard]] std::int32_t stateOf(const std::string& path) {
    const registry::BlockState* block = registry::blocks::named(path);
    REQUIRE(block != nullptr);
    return block->stateId;
}

[[nodiscard]] std::int32_t frameState(const std::string& eye, const std::string& facing) {
    const std::optional<std::int32_t> stateId =
        registry::blocks::stateIdOf(Identifier::vanilla("end_portal_frame"), {{"eye", eye}, {"facing", facing}});
    REQUIRE(stateId.has_value());
    return *stateId;
}

[[nodiscard]] WorldState emptyWorld() {
    return WorldState{{}, [](std::int32_t chunkX, std::int32_t chunkZ) { return Chunk{chunkX, chunkZ}; }};
}

/// The twelve frames of a ring, spelled out again here rather than read from the source table.
struct Slot {
    std::int32_t x = 0;
    std::int32_t z = 0;
    const char* facing = "";
};

constexpr std::array<Slot, 12> kRing{{
    {-1, -2, "south"},
    {0, -2, "south"},
    {1, -2, "south"},
    {-1, 2, "north"},
    {0, 2, "north"},
    {1, 2, "north"},
    {-2, -1, "east"},
    {-2, 0, "east"},
    {-2, 1, "east"},
    {2, -1, "west"},
    {2, 0, "west"},
    {2, 1, "west"},
}};

/// Lays the ring around a centre, leaving the slot at `unarmed` still waiting for its eye.
void placeRing(WorldState& world, std::int32_t centreX, std::int32_t y, std::int32_t centreZ,
               std::size_t unarmed = kRing.size()) {
    for (std::size_t index = 0; index < kRing.size(); ++index) {
        const Slot& slot = kRing[index];
        const std::string eye = index == unarmed ? "false" : "true";
        REQUIRE(world.setBlockState(centreX + slot.x, y, centreZ + slot.z, frameState(eye, slot.facing)));
    }
}

/// No block answers to this, which is what a slot the world cannot explain looks like.
constexpr std::int32_t kUnknownState = 1'000'000;

} // namespace

TEST_CASE("what a frame and a portal are") {
    CHECK(endPortals::isFrame(frameState("false", "north")));
    CHECK(endPortals::isFrame(frameState("true", "east")));
    CHECK_FALSE(endPortals::isFrame(registry::blocks::air().stateId));
    CHECK_FALSE(endPortals::isFrame(stateOf("end_portal")));
    CHECK_FALSE(endPortals::isFrame(stateOf("end_stone")));

    CHECK(endPortals::isEndPortal(stateOf("end_portal")));
    CHECK_FALSE(endPortals::isEndPortal(registry::blocks::air().stateId));
    CHECK_FALSE(endPortals::isEndPortal(stateOf("end_gateway")));
    CHECK_FALSE(endPortals::isEndPortal(frameState("true", "north")));
}

TEST_CASE("a frame waits for its eye") {
    for (const char* facing : {"north", "south", "west", "east"}) {
        CHECK(endPortals::needsEye(frameState("false", facing)));
        CHECK_FALSE(endPortals::needsEye(frameState("true", facing)));
    }
    CHECK_FALSE(endPortals::needsEye(registry::blocks::air().stateId));
    CHECK_FALSE(endPortals::needsEye(stateOf("obsidian")));
    CHECK_FALSE(endPortals::needsEye(stateOf("end_portal")));
}

TEST_CASE("a state the registry cannot explain is nothing at all") {
    REQUIRE_FALSE(registry::blocks::byStateId(kUnknownState).has_value());
    CHECK_FALSE(endPortals::isFrame(kUnknownState));
    CHECK_FALSE(endPortals::isEndPortal(kUnknownState));
    CHECK_FALSE(endPortals::needsEye(kUnknownState));

    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    placeRing(world, 8, 64, 8);
    REQUIRE(world.setBlockState(7, 64, 6, kUnknownState));
    CHECK(endPortals::placeEye(world, 7, 64, 6).empty());
    CHECK(endPortals::completeRing(world, 7, 64, 6).empty());
    CHECK(world.blockState(8, 64, 8) == registry::blocks::air().stateId);
}

TEST_CASE("a ring running off the loaded world stays shut") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    for (const Slot& slot : kRing) {
        // The west-facing side of this ring sits in a chunk nobody loaded.
        if (14 + slot.x > 15) {
            continue;
        }
        REQUIRE(world.setBlockState(14 + slot.x, 64, 8 + slot.z, frameState("true", slot.facing)));
    }
    CHECK(endPortals::completeRing(world, 12, 64, 7).empty());
    CHECK(world.blockState(14, 64, 8) == registry::blocks::air().stateId);
}

TEST_CASE("the eye keeps the facing the frame already had") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    REQUIRE(world.setBlockState(5, 64, 5, frameState("false", "west")));

    const std::vector<BlockChange> changed = endPortals::placeEye(world, 5, 64, 5);
    REQUIRE(changed.size() == 1);
    CHECK(changed.front() == BlockChange{5, 64, 5, frameState("true", "west"), {}});
    CHECK(world.blockState(5, 64, 5) == frameState("true", "west"));

    // A frame that already has its eye is left alone.
    CHECK(endPortals::placeEye(world, 5, 64, 5).empty());
}

TEST_CASE("nothing to arm") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    CHECK(endPortals::placeEye(world, 3, 64, 3).empty());
    REQUIRE(world.setBlockState(3, 64, 3, stateOf("obsidian")));
    CHECK(endPortals::placeEye(world, 3, 64, 3).empty());
    // Outside any loaded chunk there is no block to read.
    CHECK(endPortals::placeEye(world, 300, 64, 300).empty());
}

TEST_CASE("the last eye fills the ring") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    placeRing(world, 8, 64, 8, 0);

    const std::vector<BlockChange> changed = endPortals::placeEye(world, 7, 64, 6);
    REQUIRE(changed.size() == 10);
    CHECK(changed.front() == BlockChange{7, 64, 6, frameState("true", "south"), {}});

    const std::int32_t portal = stateOf("end_portal");
    const std::array<BlockChange, 9> expected{{
        {7, 64, 7, portal, {}},
        {7, 64, 8, portal, {}},
        {7, 64, 9, portal, {}},
        {8, 64, 7, portal, {}},
        {8, 64, 8, portal, {}},
        {8, 64, 9, portal, {}},
        {9, 64, 7, portal, {}},
        {9, 64, 8, portal, {}},
        {9, 64, 9, portal, {}},
    }};
    for (std::size_t index = 0; index < expected.size(); ++index) {
        CHECK(changed[index + 1] == expected[index]);
        CHECK(world.blockState(expected[index].x, expected[index].y, expected[index].z) == portal);
    }
    // The frames stay put, and the ring is one block thick.
    CHECK(world.blockState(6, 64, 8) == frameState("true", "east"));
    CHECK(world.blockState(6, 64, 6) == registry::blocks::air().stateId);
    CHECK(world.blockState(10, 64, 10) == registry::blocks::air().stateId);
}

TEST_CASE("a ring is found from any of its twelve frames") {
    const std::int32_t portal = stateOf("end_portal");
    for (const Slot& slot : kRing) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        placeRing(world, 8, 64, 8);
        const std::vector<BlockChange> filled = endPortals::completeRing(world, 8 + slot.x, 64, 8 + slot.z);
        CHECK(filled.size() == 9);
        CHECK(world.blockState(8, 64, 8) == portal);
    }
}

TEST_CASE("the middle of a ring is not one of its frames") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    placeRing(world, 8, 64, 8);
    CHECK(endPortals::completeRing(world, 8, 64, 8).empty());
    CHECK(world.blockState(8, 64, 8) == registry::blocks::air().stateId);
}

TEST_CASE("a frame looking the wrong way leaves the ring shut") {
    for (std::size_t index = 0; index < kRing.size(); ++index) {
        WorldState world = emptyWorld();
        world.getOrGenerateChunk(0, 0);
        placeRing(world, 8, 64, 8);
        const Slot& slot = kRing[index];
        const std::string away = std::string{slot.facing} == "south"   ? "north"
                                 : std::string{slot.facing} == "north" ? "south"
                                 : std::string{slot.facing} == "east"  ? "west"
                                                                       : "east";
        REQUIRE(world.setBlockState(8 + slot.x, 64, 8 + slot.z, frameState("true", away)));
        CHECK(endPortals::completeRing(world, 8 + kRing.front().x, 64, 8 + kRing.front().z).empty());
        CHECK(world.blockState(8, 64, 8) == registry::blocks::air().stateId);
    }
}

TEST_CASE("one frame short leaves the ring shut") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    placeRing(world, 8, 64, 8, 5);
    CHECK(endPortals::completeRing(world, 7, 64, 6).empty());
    CHECK(world.blockState(8, 64, 8) == registry::blocks::air().stateId);

    // A ring lying somewhere else entirely does not answer for this height.
    placeRing(world, 8, 64, 8);
    CHECK(endPortals::completeRing(world, 7, 65, 6).empty());
    CHECK(world.blockState(8, 65, 8) == registry::blocks::air().stateId);
    CHECK(endPortals::completeRing(world, 7, 64, 6).size() == 9);
}

TEST_CASE("the platform is obsidian with three of air over it") {
    WorldState world = emptyWorld();
    world.getOrGenerateChunk(0, 0);
    const std::int32_t stone = stateOf("stone");
    for (std::int32_t x = 4; x <= 12; ++x) {
        for (std::int32_t z = 4; z <= 12; ++z) {
            for (std::int32_t y = 60; y <= 70; ++y) {
                REQUIRE(world.setBlockState(x, y, z, stone));
            }
        }
    }

    endPortals::createEndPlatform(world, 8, 64, 8);

    const std::int32_t obsidian = stateOf("obsidian");
    const std::int32_t air = registry::blocks::air().stateId;
    for (std::int32_t offsetX = -2; offsetX <= 2; ++offsetX) {
        for (std::int32_t offsetZ = -2; offsetZ <= 2; ++offsetZ) {
            CHECK(world.blockState(8 + offsetX, 62, 8 + offsetZ) == stone);
            CHECK(world.blockState(8 + offsetX, 63, 8 + offsetZ) == obsidian);
            CHECK(world.blockState(8 + offsetX, 64, 8 + offsetZ) == air);
            CHECK(world.blockState(8 + offsetX, 65, 8 + offsetZ) == air);
            CHECK(world.blockState(8 + offsetX, 66, 8 + offsetZ) == air);
            CHECK(world.blockState(8 + offsetX, 67, 8 + offsetZ) == stone);
        }
    }
    // Five by five and no wider.
    CHECK(world.blockState(5, 63, 8) == stone);
    CHECK(world.blockState(11, 63, 8) == stone);
    CHECK(world.blockState(8, 63, 5) == stone);
    CHECK(world.blockState(8, 63, 11) == stone);
}

TEST_CASE("the platform generates the chunks it lands on") {
    WorldState world = emptyWorld();
    endPortals::createEndPlatform(world, endPortals::kSpawnX, endPortals::kSpawnY, endPortals::kSpawnZ);
    CHECK(world.loadedChunkCount() == 2);
    const std::int32_t obsidian = stateOf("obsidian");
    // One corner in each of the two chunks the five by five straddles.
    CHECK(world.blockState(98, 49, -2) == obsidian);
    CHECK(world.blockState(102, 49, 2) == obsidian);

    // And again where it is the z side that straddles the border.
    WorldState other = emptyWorld();
    endPortals::createEndPlatform(other, 8, 64, 30);
    CHECK(other.loadedChunkCount() == 2);
    CHECK(other.blockState(6, 63, 28) == obsidian);
    CHECK(other.blockState(10, 63, 32) == obsidian);
}

TEST_CASE("where the portal leaves you") {
    WorldState world = emptyWorld();
    const endPortals::Exit home = endPortals::exitFor(world, true, 1.5, 70.0, 2.5);
    CHECK(home == endPortals::Exit{1.5, 70.0, 2.5});
    // Coming home builds nothing.
    CHECK(world.loadedChunkCount() == 0);

    const endPortals::Exit away = endPortals::exitFor(world, false, 1.5, 70.0, 2.5);
    CHECK(away == endPortals::Exit{100.5, 50.0, 0.5});
    CHECK(world.blockState(100, 49, 0) == stateOf("obsidian"));
    CHECK(world.blockState(100, 50, 0) == registry::blocks::air().stateId);
}
