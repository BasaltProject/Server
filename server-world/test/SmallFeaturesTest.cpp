#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/SmallFeatures.hpp>

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

/// Grass over dirt with sand, a dirt path and a stone ceiling, one rule each.
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
            switch (floorMod(worldX * 5 + worldZ * 3, 6)) {
            case 0:
                top = stateIdOf("sand");
                break;
            // A dirt path holds a pile only half the time, which no other block does.
            case 1:
                top = stateIdOf("dirt_path");
                break;
            case 2:
                top = stateIdOf("moss_block");
                break;
            default:
                break;
            }
            chunk.setBlockStateId(x, 70, z, top);
            // A ceiling four blocks up, which is what a spore blossom hangs from.
            if (floorMod(worldX * 7 + worldZ, 4) == 0) {
                chunk.setBlockStateId(x, 75, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> ignored = {"dirt",       "grass_block", "sand", "dirt_path",
                                                               "moss_block", "stone",       "air"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = 60; y <= 85; ++y) {
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
    std::string known;
    std::int32_t placed = 0;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
};

[[nodiscard]] Run run(std::string_view feature) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t chunkIndex = 0; chunkIndex < 4; ++chunkIndex) {
        Chunk chunk = groundChunk(chunkIndex, -chunkIndex);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(2503LL + chunkIndex * 977LL));
        // The spore blossom hangs, so it needs air under the ceiling instead of ground.
        const std::int32_t y = feature == "spore_blossom" ? 74 : 71;
        const DecorationPos origin{.x = chunk.chunkX() * 16 + 8, .y = y, .z = chunk.chunkZ() * 16 + 8};
        result.known += smallFeatures::placeAt(chunk, random, origin, feature) ? "1" : "0";
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("every small feature places what the frozen kotlin places") {
    SUBCASE("patch_firefly_bush_near_water") {
        const Run result = run("patch_firefly_bush_near_water");
        CHECK(result.known == "1111");
        CHECK(result.placed == 49);
        CHECK(result.checksum == -7701576693695337106LL);
        CHECK(result.after == -1670531482);
    }

    SUBCASE("patch_firefly_bush_near_water_swamp") {
        const Run result = run("patch_firefly_bush_near_water_swamp");
        CHECK(result.known == "1111");
        CHECK(result.placed == 49);
        CHECK(result.checksum == -7701576693695337106LL);
        CHECK(result.after == -1670531482);
    }

    SUBCASE("patch_firefly_bush_swamp") {
        const Run result = run("patch_firefly_bush_swamp");
        CHECK(result.known == "1111");
        CHECK(result.placed == 49);
        CHECK(result.checksum == -7701576693695337106LL);
        CHECK(result.after == -1670531482);
    }

    SUBCASE("patch_leaf_litter") {
        const Run result = run("patch_leaf_litter");
        CHECK(result.known == "1111");
        CHECK(result.placed == 63);
        CHECK(result.checksum == 3460478464486807508LL);
        CHECK(result.after == -241829251);
    }

    SUBCASE("patch_sugar_cane") {
        const Run result = run("patch_sugar_cane");
        CHECK(result.known == "1111");
        CHECK(result.placed == 10);
        CHECK(result.checksum == -2052542142443319353LL);
        CHECK(result.after == -1173546869);
    }

    SUBCASE("patch_sugar_cane_badlands") {
        const Run result = run("patch_sugar_cane_badlands");
        CHECK(result.known == "1111");
        CHECK(result.placed == 10);
        CHECK(result.checksum == -2052542142443319353LL);
        CHECK(result.after == -1173546869);
    }

    SUBCASE("patch_sugar_cane_desert") {
        const Run result = run("patch_sugar_cane_desert");
        CHECK(result.known == "1111");
        CHECK(result.placed == 10);
        CHECK(result.checksum == -2052542142443319353LL);
        CHECK(result.after == -1173546869);
    }

    SUBCASE("patch_sugar_cane_swamp") {
        const Run result = run("patch_sugar_cane_swamp");
        CHECK(result.known == "1111");
        CHECK(result.placed == 10);
        CHECK(result.checksum == -2052542142443319353LL);
        CHECK(result.after == -1173546869);
    }

    SUBCASE("pile_hay") {
        const Run result = run("pile_hay");
        CHECK(result.known == "1111");
        CHECK(result.placed == 51);
        CHECK(result.checksum == -3348370156464482892LL);
        CHECK(result.after == 1537663053);
    }

    SUBCASE("pile_ice") {
        const Run result = run("pile_ice");
        CHECK(result.known == "1111");
        CHECK(result.placed == 51);
        CHECK(result.checksum == -6824392487982622795LL);
        CHECK(result.after == 1537663053);
    }

    SUBCASE("pile_melon") {
        const Run result = run("pile_melon");
        CHECK(result.known == "1111");
        CHECK(result.placed == 49);
        CHECK(result.checksum == 6629841366777137210LL);
        CHECK(result.after == -1673786261);
    }

    SUBCASE("pile_pumpkin") {
        const Run result = run("pile_pumpkin");
        CHECK(result.known == "1111");
        CHECK(result.placed == 51);
        CHECK(result.checksum == 3965145527266213713LL);
        CHECK(result.after == 1537663053);
    }

    SUBCASE("pile_snow") {
        const Run result = run("pile_snow");
        CHECK(result.known == "1111");
        CHECK(result.placed == 36);
        CHECK(result.checksum == 6227630941023671327LL);
        CHECK(result.after == -1673786261);
    }

    SUBCASE("spore_blossom") {
        const Run result = run("spore_blossom");
        CHECK(result.known == "1111");
        CHECK(result.placed == 4);
        CHECK(result.checksum == -2939156266347186843LL);
        CHECK(result.after == 116495287);
    }

    SUBCASE("wildflowers_birch_forest") {
        const Run result = run("wildflowers_birch_forest");
        CHECK(result.known == "1111");
        CHECK(result.placed == 49);
        CHECK(result.checksum == -3200092733811139798LL);
        CHECK(result.after == -241829251);
    }

    SUBCASE("wildflowers_meadow") {
        const Run result = run("wildflowers_meadow");
        CHECK(result.known == "1111");
        CHECK(result.placed == 49);
        CHECK(result.checksum == -3200092733811139798LL);
        CHECK(result.after == -241829251);
    }

    SUBCASE("not_a_feature") {
        const Run result = run("not_a_feature");
        CHECK(result.known == "0000");
        CHECK(result.placed == 0);
        CHECK(result.checksum == -8602589583363825039LL);
        CHECK(result.after == 116495287);
    }
}
