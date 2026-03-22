#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/FlowerVegetation.hpp>
#include <basalt/server/world/terrain/decoration/GrassScatterPatches.hpp>
#include <basalt/server/world/terrain/decoration/GroundVegetation.hpp>
#include <basalt/server/world/terrain/decoration/JungleFeatures.hpp>
#include <basalt/server/world/terrain/decoration/SurfacePatches.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ, std::string_view surface, bool pond,
                                bool wall) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf(surface));
        }
    }
    // A vine needs a vertical face to hang from, which flat ground never gives it.
    if (wall) {
        for (std::int32_t y = 71; y <= 100; ++y) {
            for (std::int32_t x = 7; x <= 8; ++x) {
                chunk.setBlockStateId(x, y, 8, registry::blocks::stone().stateId);
            }
        }
    }
    if (pond) {
        for (std::int32_t x = 3; x <= 12; ++x) {
            for (std::int32_t z = 3; z <= 12; ++z) {
                chunk.setBlockStateId(x, 70, z, registry::blocks::water().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ignored = {"stone", "air", "water", "grass_block"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 60; y <= 90; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && !ignored.contains(block->path())) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

struct Run {
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    std::size_t ticks = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

using Body = std::function<void(Chunk&, rng::RandomSource&, const DecorationPos&,
                                const testsupport::FixedBiomeEnvironment&, std::int64_t)>;

[[nodiscard]] Run run(std::string_view surface, bool pond, bool wall, std::int32_t count, const Body& body) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < count; ++index) {
        Chunk chunk = groundChunk(index * 5 - 30, index * -3 + 12, surface, pond, wall);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(1234LL + index * 3719LL));
        const DecorationPos origin{.x = chunk.chunkX() * 16 + 8, .y = 71, .z = chunk.chunkZ() * 16 + 8};
        const testsupport::FixedBiomeEnvironment environment{"minecraft:swamp"};
        body(chunk, random, origin, environment, 5150LL + index);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.ticks += chunk.scheduledTickCount();
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the small vegetation patches place what the frozen kotlin places") {
    SUBCASE("flowerDefault") {
        const Run result = run("grass_block", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin,
                                  const testsupport::FixedBiomeEnvironment&, std::int64_t) {
                                   defaultVegetationDecoration::placeDefaultPatchAt(chunk, random, origin);
                               });
        CHECK(result.placed == 481);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 3015017640969685622LL);
        CHECK(result.after == 1501487701);
    }

    SUBCASE("dryGrassAt") {
        const Run result = run("sand", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { dryGrassDecoration::placeAt(chunk, random, origin); });
        CHECK(result.placed == 8161);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -8635276990260364833LL);
        CHECK(result.after == 1501487701);
    }

    SUBCASE("dryGrassDesert") {
        const Run result = run("sand", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos&,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { dryGrassDecoration::desert()(chunk, random); });
        CHECK(result.placed == 7779);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -3374951737952660815LL);
        CHECK(result.after == -331529504);
    }

    SUBCASE("dryGrassBadlands") {
        const Run result = run("red_sand", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos&,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { dryGrassDecoration::badlands()(chunk, random); });
        CHECK(result.placed == 7760);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -1448033722498214410LL);
        CHECK(result.after == -331529504);
    }

    SUBCASE("forestFlowersAt") {
        const Run result = run("grass_block", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { forestFlowerDecoration::placeAt(chunk, random, origin); });
        CHECK(result.placed == 1155);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 6513867751577363571LL);
        CHECK(result.after == 2001131377);
    }

    SUBCASE("forestFlowersNormal") {
        const Run result = run("grass_block", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos&,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { forestFlowerDecoration::normal()(chunk, random); });
        CHECK(result.placed == 24);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -1013793404078361863LL);
        CHECK(result.after == 1375486844);
    }

    SUBCASE("forestFlowersFlowerForest") {
        const Run result = run("grass_block", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos&,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { forestFlowerDecoration::flowerForest()(chunk, random); });
        CHECK(result.placed == 133);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -2725034566552775978LL);
        CHECK(result.after == 1375486844);
    }

    SUBCASE("jungleVines") {
        const Run result = run("grass_block", false, true, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos&,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { jungleVineDecoration::decoration()(chunk, random); });
        CHECK(result.placed == 41);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 1807402872832646133LL);
        CHECK(result.after == -1187441586);
    }

    SUBCASE("jungleVineAt") {
        const Run result =
            run("grass_block", false, true, 30,
                [](Chunk& chunk, rng::RandomSource&, const DecorationPos&, const testsupport::FixedBiomeEnvironment&,
                   std::int64_t) {
                    for (std::int32_t y = 72; y <= 90; ++y) {
                        static_cast<void>(jungleVineDecoration::placeAt(
                            chunk, DecorationPos{.x = chunk.chunkX() * 16 + 9, .y = y, .z = chunk.chunkZ() * 16 + 8}));
                    }
                });
        CHECK(result.placed == 570);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -2143252860842785713LL);
        CHECK(result.after == 302298358);
    }

    SUBCASE("waterlilyAt") {
        const Run result = run("grass_block", true, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { waterlilyPatchDecoration::placeAt(chunk, random, origin); });
        CHECK(result.placed == 55);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -7958714299179860907LL);
        CHECK(result.after == 1294296077);
    }

    SUBCASE("waterlilyStep") {
        const Run result = run("grass_block", true, false, 30,
                               [](Chunk& chunk, rng::RandomSource&, const DecorationPos&,
                                  const testsupport::FixedBiomeEnvironment& environment,
                                  std::int64_t seed) { waterlilyPatchDecoration::decorate(chunk, seed, environment); });
        CHECK(result.placed == 90);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 8221607680703210003LL);
        CHECK(result.after == 302298358);
    }

    SUBCASE("pumpkinAt") {
        const Run result = run("grass_block", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin,
                                  const testsupport::FixedBiomeEnvironment&,
                                  std::int64_t) { pumpkinPatchDecoration::placeAt(chunk, random, origin); });
        CHECK(result.placed == 657);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == -8064671641445176915LL);
        CHECK(result.after == 1793259573);
    }

    SUBCASE("pumpkinStep") {
        const Run result = run("grass_block", false, false, 900,
                               [](Chunk& chunk, rng::RandomSource&, const DecorationPos&,
                                  const testsupport::FixedBiomeEnvironment& environment,
                                  std::int64_t seed) { pumpkinPatchDecoration::decorate(chunk, seed, environment); });
        CHECK(result.placed == 42);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 642475953172578749LL);
        CHECK(result.after == -1732597536);
    }

    SUBCASE("bareStone") {
        const Run result = run("stone", false, false, 30,
                               [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin,
                                  const testsupport::FixedBiomeEnvironment&, std::int64_t) {
                                   defaultVegetationDecoration::placeDefaultPatchAt(chunk, random, origin);
                               });
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.ticks == 0);
        CHECK(result.checksum == 1549579132615385875LL);
        CHECK(result.after == 1501487701);
    }
}
