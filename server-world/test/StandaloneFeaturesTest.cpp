#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/StandaloneFeatures.hpp>

#include <doctest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

enum class Ground : std::uint8_t { Solid, Cave, Corridor };

/// Solid rock of one kind, optionally hollowed: a wide cave, or a narrow corridor. A monster room
/// needs a solid floor and ceiling with a handful of open wall squares, which only a corridor gives.
[[nodiscard]] Chunk rockChunk(std::int32_t chunkX, std::int32_t chunkZ, std::string_view rock, Ground ground) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y <= 60; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf(rock));
            }
        }
    }
    if (ground == Ground::Cave) {
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t y = 30; y <= 35; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
                }
            }
        }
    } else if (ground == Ground::Corridor) {
        for (std::int32_t x = 0; x < 16; ++x) {
            for (std::int32_t z = 7; z <= 8; ++z) {
                for (std::int32_t y = 31; y <= 32; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::int64_t checksumOf(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t y = 0; y <= 64; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                checksum =
                    (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(chunk.blockStateId(x, y, z)))) *
                    1099511628211ULL;
            }
        }
    }
    return static_cast<std::int64_t>(checksum);
}

struct Run {
    std::string placed;
    std::size_t deferred = 0;
    std::size_t entities = 0;
    std::size_t ticks = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(std::string_view feature, std::int32_t originY, std::string_view rock, Ground ground,
                      std::int32_t localX, std::int32_t localZ, bool withNeighbourTerrain) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 20; ++index) {
        Chunk chunk = rockChunk(index * 5 - 30, index * -3 + 12, rock, ground);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(613LL + index * 4409LL));
        const DecorationPos origin{.x = chunk.chunkX() * 16 + localX, .y = originY, .z = chunk.chunkZ() * 16 + localZ};
        bool placed = false;
        if (withNeighbourTerrain) {
            // Outside the chunk the decoration view reads as air, which makes the shape checks reject
            // at an edge. Real generation hands them the neighbour's terrain instead.
            const Chunk::ExternalStateScope terrain{
                chunk, [rock](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                    return y >= 0 && y <= 60 ? stateIdOf(rock) : registry::blocks::air().stateId;
                }};
            placed = standaloneFeatures::placeAt(chunk, random, origin, feature);
        } else {
            placed = standaloneFeatures::placeAt(chunk, random, origin, feature);
        }
        result.placed += placed ? "1" : "0";
        checksum = (checksum ^ static_cast<std::uint64_t>(checksumOf(chunk))) * 1099511628211ULL;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.entities += chunk.blockEntityCount();
        result.ticks += chunk.scheduledTickCount() + chunk.takeGeneratedBlockTicks().size();
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the standalone features build what the frozen kotlin builds") {
    SUBCASE("springClosed") {
        const Run result = run("spring_closed", 25, "netherrack", Ground::Solid, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 20);
        CHECK(result.checksum == 6716135181350909685LL);
        CHECK(result.after == 1499221538);
    }

    SUBCASE("springClosedDouble") {
        const Run result = run("spring_closed_double", 25, "netherrack", Ground::Solid, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 20);
        CHECK(result.checksum == 6716135181350909685LL);
        CHECK(result.after == 1499221538);
    }

    SUBCASE("monsterRoom") {
        const Run result = run("monster_room", 31, "stone", Ground::Corridor, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 49);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 6244967410082277792LL);
        CHECK(result.after == -1789018993);
    }

    SUBCASE("monsterRoomDeep") {
        const Run result = run("monster_room_deep", 31, "stone", Ground::Corridor, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 49);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 6244967410082277792LL);
        CHECK(result.after == -1789018993);
    }

    SUBCASE("monsterRoomEdge") {
        const Run result = run("monster_room", 31, "stone", Ground::Corridor, 1, 8, true);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 2110);
        CHECK(result.entities == 44);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -2625562516512525695LL);
        CHECK(result.after == -1789018993);
    }

    SUBCASE("endPlatform") {
        const Run result = run("end_platform", 32, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 8186997229022751541LL);
        CHECK(result.after == 1499221538);
    }

    SUBCASE("voidStartPlatform") {
        const Run result = run("void_start_platform", 32, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 3493720867087958177LL);
        CHECK(result.after == 1499221538);
    }

    SUBCASE("endGatewayReturn") {
        const Run result = run("end_gateway_return", 32, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 20);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -7169104649194670623LL);
        CHECK(result.after == 1499221538);
    }

    SUBCASE("lakeLavaSurface") {
        const Run result = run("lake_lava_surface", 20, "stone", Ground::Solid, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -3260000313910670810LL);
        CHECK(result.after == 379264702);
    }

    SUBCASE("lakeLavaUnderground") {
        const Run result = run("lake_lava_underground", 20, "stone", Ground::Solid, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -3260000313910670810LL);
        CHECK(result.after == 379264702);
    }

    SUBCASE("lakeLavaEdge") {
        const Run result = run("lake_lava_surface", 20, "stone", Ground::Solid, 2, 13, true);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 2287);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -2730246052395800981LL);
        CHECK(result.after == 379264702);
    }

    SUBCASE("caveVines") {
        const Run result = run("cave_vines", 34, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -8287974544193943863LL);
        CHECK(result.after == 1932267493);
    }

    SUBCASE("classicVines") {
        const Run result = run("classic_vines_cave_feature", 35, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 3247040537533737077LL);
        CHECK(result.after == 1499221538);
    }

    SUBCASE("lushVegetation") {
        const Run result = run("lush_caves_vegetation", 32, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -9036528739786843003LL);
        CHECK(result.after == 1105996800);
    }

    SUBCASE("lushCeilingVegetation") {
        const Run result = run("lush_caves_ceiling_vegetation", 34, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -7522590554827546754LL);
        CHECK(result.after == -479895210);
    }

    SUBCASE("lushClay") {
        const Run result = run("lush_caves_clay", 32, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -6276291529507889507LL);
        CHECK(result.after == 525147351);
    }

    SUBCASE("rootedAzaleaTree") {
        const Run result = run("rooted_azalea_tree", 31, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "11111111111111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -4872262014636135603LL);
        CHECK(result.after == 276213909);
    }

    SUBCASE("sulfurPool") {
        const Run result = run("sulfur_pool", 20, "stone", Ground::Solid, 8, 8, false);
        CHECK(result.placed == "11110111011111111111");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -8392339967193549722LL);
        CHECK(result.after == 379264702);
    }

    SUBCASE("monsterRoomSolid") {
        const Run result = run("monster_room", 31, "stone", Ground::Solid, 8, 8, false);
        CHECK(result.placed == "00000000000000000000");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -4294274760425460575LL);
        CHECK(result.after == -481811466);
    }

    SUBCASE("unknownFeature") {
        const Run result = run("unknown_feature", 32, "stone", Ground::Cave, 8, 8, false);
        CHECK(result.placed == "00000000000000000000");
        CHECK(result.deferred == 0);
        CHECK(result.entities == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 3493720867087958177LL);
        CHECK(result.after == 1499221538);
    }
}
