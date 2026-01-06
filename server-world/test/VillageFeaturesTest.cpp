#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/VillageFeatures.hpp>

#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

/// Village ground: grass, farmland, sand and the dirt path a pile only half settles on.
[[nodiscard]] Chunk groundChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            for (std::int32_t y = 60; y <= 69; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("dirt"));
            }
            std::int32_t top = registry::blocks::named("grass_block")->stateId;
            switch (floorMod(worldX * 3 + worldZ * 5, 7)) {
            case 0:
                top = stateIdOf("dirt_path");
                break;
            case 1:
                top = stateIdOf("sand");
                break;
            case 2:
                top = stateIdOf("farmland");
                break;
            case 3:
                top = registry::blocks::stone().stateId;
                break;
            default:
                break;
            }
            chunk.setBlockStateId(x, 70, z, top);
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ignored = {"dirt",     "grass_block", "sand", "dirt_path",
                                                               "farmland", "stone",       "air"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 60; y <= 110; ++y) {
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
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(std::string_view feature) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t chunkIndex = 0; chunkIndex < 4; ++chunkIndex) {
        Chunk chunk = groundChunk(chunkIndex * 2 - 3, chunkIndex + 1);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(1801LL + chunkIndex * 2711LL));
        villageFeatures::place(feature, chunk, chunk.chunkX() * 16 + 8, 71, chunk.chunkZ() * 16 + 8, random);
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("every village pool feature places what the frozen kotlin places") {
    SUBCASE("minecraft:pile_hay") {
        const Run result = run("minecraft:pile_hay");
        CHECK(result.placed == 42);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5337228931624283299LL);
        CHECK(result.after == 1402209232);
    }

    SUBCASE("minecraft:pile_melon") {
        const Run result = run("minecraft:pile_melon");
        CHECK(result.placed == 53);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4687383055195050442LL);
        CHECK(result.after == 554997071);
    }

    SUBCASE("minecraft:pile_snow") {
        const Run result = run("minecraft:pile_snow");
        CHECK(result.placed == 33);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 7468114353759756740LL);
        CHECK(result.after == 554997071);
    }

    SUBCASE("minecraft:pile_ice") {
        const Run result = run("minecraft:pile_ice");
        CHECK(result.placed == 42);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4330642807767456145LL);
        CHECK(result.after == 1402209232);
    }

    SUBCASE("minecraft:pile_pumpkin") {
        const Run result = run("minecraft:pile_pumpkin");
        CHECK(result.placed == 42);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6318093642972972149LL);
        CHECK(result.after == 1402209232);
    }

    SUBCASE("minecraft:acacia") {
        const Run result = run("minecraft:acacia");
        CHECK(result.placed == 79);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 1876341715946158099LL);
        CHECK(result.after == -582578140);
    }

    SUBCASE("minecraft:oak") {
        const Run result = run("minecraft:oak");
        CHECK(result.placed == 58);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 63661520530376354LL);
        CHECK(result.after == -582578140);
    }

    SUBCASE("minecraft:spruce") {
        const Run result = run("minecraft:spruce");
        CHECK(result.placed == 64);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6979621724745050119LL);
        CHECK(result.after == -582578140);
    }

    SUBCASE("minecraft:pine") {
        const Run result = run("minecraft:pine");
        CHECK(result.placed == 37);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8119617379623644372LL);
        CHECK(result.after == -582578140);
    }

    SUBCASE("minecraft:flower_plain") {
        const Run result = run("minecraft:flower_plain");
        CHECK(result.placed == 47);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 8981657393775653698LL);
        CHECK(result.after == 954366245);
    }

    SUBCASE("minecraft:patch_cactus") {
        const Run result = run("minecraft:patch_cactus");
        CHECK(result.placed == 1);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -1675501890901952608LL);
        CHECK(result.after == 1039304233);
    }

    SUBCASE("minecraft:patch_taiga_grass") {
        const Run result = run("minecraft:patch_taiga_grass");
        CHECK(result.placed == 19);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5675209386077902576LL);
        CHECK(result.after == 1747964721);
    }

    SUBCASE("minecraft:patch_berry_bush") {
        const Run result = run("minecraft:patch_berry_bush");
        CHECK(result.placed == 46);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -5201678230457354627LL);
        CHECK(result.after == 134324992);
    }

    SUBCASE("minecraft:not_a_feature") {
        const Run result = run("minecraft:not_a_feature");
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 6114860990693194429LL);
        CHECK(result.after == -582578140);
    }
}
