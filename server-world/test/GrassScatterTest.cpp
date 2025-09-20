#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/GrassScatterPatches.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk grassChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            // Only the layers these patches actually read: they touch the origin and the block under
            // it, never deeper, and filling the whole column dominated the run time.
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 69; y <= 76; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (!registry::blocks::isAir(id) && id != registry::blocks::stone().stateId &&
                    id != stateIdOf("grass_block")) {
                    ++placed;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), placed};
}

/// Over many chunks with spaced seeds: rarity here runs to 1/384 and 1/300, and sequential legacy
/// seeds leave the very draw those filters read correlated.
[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 600; ++index) {
        Chunk chunk = grassChunk(index * 3 - 100, index * -7 + 50);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        placed += chunkPlaced;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {placed, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the berry and pumpkin patches place what the frozen kotlin places") {
    SUBCASE("berryTaiga") {
        const auto [placed, deferred, checksum] =
            run("minecraft:taiga", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                berryPatchDecoration::decorate(chunk, 606060LL, environment);
            });
        CHECK(placed == 278);
        CHECK(deferred == 0);
        CHECK(checksum == -473324326971594819LL);
    }

    SUBCASE("berrySnowy") {
        const auto [placed, deferred, checksum] =
            run("minecraft:snowy_taiga", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                berryPatchDecoration::decorate(chunk, 606060LL, environment);
            });
        CHECK(placed == 14);
        CHECK(deferred == 0);
        CHECK(checksum == -1503199922215968675LL);
    }

    SUBCASE("berryWrong") {
        const auto [placed, deferred, checksum] =
            run("minecraft:desert", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                berryPatchDecoration::decorate(chunk, 606060LL, environment);
            });
        CHECK(placed == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 293482985887311005LL);
    }

    SUBCASE("berryCommonOnly") {
        const auto [placed, deferred, checksum] =
            run("minecraft:taiga", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                berryPatchDecoration::decorateFeature(chunk, 606060LL, environment, "patch_berry_common");
            });
        CHECK(placed == 278);
        CHECK(deferred == 0);
        CHECK(checksum == -473324326971594819LL);
    }

    SUBCASE("pumpkin") {
        const auto [placed, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                pumpkinPatchDecoration::decorate(chunk, 606060LL, environment);
            });
        CHECK(placed == 25);
        CHECK(deferred == 0);
        CHECK(checksum == -7426862055943998727LL);
    }

    SUBCASE("pumpkinWrong") {
        // mushroom_fields is genuinely absent from the pumpkin list; desert is in it, so using
        // desert here would have tested acceptance twice and rejection never.
        const auto [placed, deferred, checksum] =
            run("minecraft:mushroom_fields", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                pumpkinPatchDecoration::decorate(chunk, 606060LL, environment);
            });
        CHECK(placed == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 293482985887311005LL);
    }

    SUBCASE("berryRegional") {
        const auto [placed, deferred, checksum] =
            run("minecraft:taiga", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                berryPatchDecoration::decorate(
                    chunk, 606060LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y < 70) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("grass_block") : registry::blocks::air().stateId;
                    });
            });
        CHECK(placed == 278);
        CHECK(deferred == 113);
        CHECK(checksum == -473324326971594819LL);
    }
}

TEST_CASE("one berry and one pumpkin patch at an origin match the frozen kotlin") {
    Chunk chunk = grassChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(90210LL));
    berryPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8});
    pumpkinPatchDecoration::placeAt(chunk, random, DecorationPos{4, 71, 12});
    CHECK(fold(chunk).second == 43);
    CHECK(fold(chunk).first == -2583229570941417999LL);
    CHECK(random.nextInt() == -335304423);
}
