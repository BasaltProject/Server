#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/LushVegetationPatchFeature.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// A cave: solid stone with a hollow band whose floor is uneven, so the ground walk, the ceiling
/// walk and the waterlogged enclosure check all have something to find.
[[nodiscard]] Chunk caveChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 0; y <= 40; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            const std::int32_t floor = 20 + (x + z) % 3;
            for (std::int32_t y = floor; y <= floor + 6; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
            }
        }
    }
    return chunk;
}

struct Counts {
    std::int32_t moss = 0;
    std::int32_t clay = 0;
    std::int32_t plants = 0;
    std::int32_t dripleaf = 0;
    std::int32_t water = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Counts fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> plantPaths = {
        "moss_carpet", "short_grass", "tall_grass", "azalea", "flowering_azalea", "cave_vines", "cave_vines_plant"};
    static const std::set<std::string, std::less<>> dripleafPaths = {"small_dripleaf", "big_dripleaf",
                                                                     "big_dripleaf_stem"};
    std::uint64_t checksum = 14695981039346656037ULL;
    Counts result;
    for (std::int32_t y = 0; y <= 45; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr) {
                    continue;
                }
                const std::string path = block->path();
                if (path == "moss_block") {
                    ++result.moss;
                } else if (path == "clay") {
                    ++result.clay;
                } else if (path == "water") {
                    ++result.water;
                } else if (plantPaths.contains(path)) {
                    ++result.plants;
                } else if (dripleafPaths.contains(path)) {
                    ++result.dripleaf;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

struct Run {
    std::string placed;
    Counts totals;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

using Body = std::function<bool(Chunk&, rng::RandomSource&, const DecorationPos&)>;

[[nodiscard]] Run run(std::int32_t originY, std::int32_t count, const Body& body) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < count; ++index) {
        Chunk chunk = caveChunk(index * 5 - 30, index * -3 + 12);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(97LL + index * 6151LL));
        const DecorationPos origin{.x = chunk.chunkX() * 16 + 8, .y = originY, .z = chunk.chunkZ() * 16 + 8};
        result.placed += body(chunk, random, origin) ? "1" : "0";
        const Counts counts = fold(chunk);
        result.totals.moss += counts.moss;
        result.totals.clay += counts.clay;
        result.totals.plants += counts.plants;
        result.totals.dripleaf += counts.dripleaf;
        result.totals.water += counts.water;
        checksum = (checksum ^ static_cast<std::uint64_t>(counts.checksum)) * 1099511628211ULL;
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the lush cave patches place what the frozen kotlin places") {
    SUBCASE("floorMoss") {
        const Run result = run(22, 24, [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin) {
            return lushVegetationPatchFeature::placeFloorMoss(chunk, random, origin);
        });
        CHECK(result.placed == "111111111111111111111111");
        CHECK(result.totals.moss == 4098);
        CHECK(result.totals.clay == 0);
        CHECK(result.totals.plants == 3632);
        CHECK(result.totals.dripleaf == 0);
        CHECK(result.totals.water == 0);
        CHECK(result.checksum == -2647819465792394605LL);
        CHECK(result.after == -2096034089);
    }

    SUBCASE("ceilingMoss") {
        const Run result = run(25, 24, [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin) {
            return lushVegetationPatchFeature::placeCeilingMoss(chunk, random, origin);
        });
        CHECK(result.placed == "111111111111111111111111");
        CHECK(result.totals.moss == 6095);
        CHECK(result.totals.clay == 0);
        CHECK(result.totals.plants == 843);
        CHECK(result.totals.dripleaf == 0);
        CHECK(result.totals.water == 0);
        CHECK(result.checksum == -7802049353988737018LL);
        CHECK(result.after == -99318013);
    }

    SUBCASE("clay") {
        const Run result = run(22, 24, [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin) {
            return lushVegetationPatchFeature::placeClay(chunk, random, origin);
        });
        CHECK(result.placed == "111111111111111111111111");
        CHECK(result.totals.moss == 0);
        CHECK(result.totals.clay == 16174);
        CHECK(result.totals.plants == 0);
        CHECK(result.totals.dripleaf == 464);
        CHECK(result.totals.water == 637);
        CHECK(result.checksum == 8854811118268897032LL);
        CHECK(result.after == 1664725639);
    }

    SUBCASE("clayDeep") {
        const Run result = run(21, 48, [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin) {
            return lushVegetationPatchFeature::placeClay(chunk, random, origin);
        });
        CHECK(result.placed == "111111111111111111111111111111111111111111111111");
        CHECK(result.totals.moss == 0);
        CHECK(result.totals.clay == 31805);
        CHECK(result.totals.plants == 0);
        CHECK(result.totals.dripleaf == 884);
        CHECK(result.totals.water == 1137);
        CHECK(result.checksum == -8159288940778800465LL);
        CHECK(result.after == 1048366848);
    }

    SUBCASE("dripleafAt") {
        const Run result = run(23, 24, [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin) {
            return lushVegetationPatchFeature::placeDripleafAt(chunk, random, origin);
        });
        CHECK(result.placed == "101111111111110111101001");
        CHECK(result.totals.moss == 0);
        CHECK(result.totals.clay == 0);
        CHECK(result.totals.plants == 0);
        CHECK(result.totals.dripleaf == 34);
        CHECK(result.totals.water == 0);
        CHECK(result.checksum == 3176987768055681799LL);
        CHECK(result.after == -944496466);
    }

    SUBCASE("solid") {
        const Run result = run(5, 24, [](Chunk& chunk, rng::RandomSource& random, const DecorationPos& origin) {
            return lushVegetationPatchFeature::placeFloorMoss(chunk, random, origin);
        });
        CHECK(result.placed == "000000000000000000000000");
        CHECK(result.totals.moss == 0);
        CHECK(result.totals.clay == 0);
        CHECK(result.totals.plants == 0);
        CHECK(result.totals.dripleaf == 0);
        CHECK(result.totals.water == 0);
        CHECK(result.checksum == 4677117039963092701LL);
        CHECK(result.after == -97405794);
    }
}
