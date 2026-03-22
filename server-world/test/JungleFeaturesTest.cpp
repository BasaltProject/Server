#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/JungleFeatures.hpp>

#include <doctest.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

/// Grass floor with a soft cover stripe, and stone pillars up through the vine height band: the
/// melon replaces more than air, and a vine needs a solid face beside open air.
[[nodiscard]] Chunk jungleChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
            if (z % 4 == 0) {
                chunk.setBlockStateId(x, 71, z, stateIdOf("short_grass"));
            }
            if (x % 5 == 0 && z % 5 == 0) {
                for (std::int32_t y = 71; y <= 102; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::int32_t> fold(const Chunk& chunk) {
    testsupport::Checksum checksum;
    std::int32_t melons = 0;
    std::int32_t vines = 0;
    for (std::int32_t y = 60; y <= 104; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum.feed(id);
                if (id == stateIdOf("melon")) {
                    ++melons;
                }
                if (registry::blockStateFamilies::isVineState(id)) {
                    ++vines;
                }
            }
        }
    }
    return {checksum.value(), melons, vines};
}

/// The chunk count is per case: the sparse melon is 1/64, so it needs a far wider sample to fire.
[[nodiscard]] std::tuple<std::int32_t, std::int32_t, std::size_t, std::int64_t>
run(std::int32_t chunks, const std::function<void(Chunk&, basalt::server::random::RandomSource&)>& body) {
    testsupport::Checksum checksum;
    std::int32_t melons = 0;
    std::int32_t vines = 0;
    std::set<std::int32_t> distinct;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = jungleChunk(index * 3 - 30, index * -5 + 10);
        basalt::server::random::RandomSource random =
            basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(
                static_cast<std::int64_t>(717171ULL + static_cast<std::uint64_t>(index) * 6364136223846793005ULL)));
        body(chunk, random);
        const auto [chunkChecksum, chunkMelons, chunkVines] = fold(chunk);
        checksum.feed(chunkChecksum);
        melons += chunkMelons;
        vines += chunkVines;
        for (std::int32_t y = 60; y <= 104; ++y) {
            for (std::int32_t z = 0; z < 16; ++z) {
                for (std::int32_t x = 0; x < 16; ++x) {
                    const std::int32_t id = chunk.blockStateId(x, y, z);
                    if (registry::blockStateFamilies::isVineState(id)) {
                        distinct.insert(id);
                    }
                }
            }
        }
    }
    return {melons, vines, distinct.size(), checksum.value()};
}

} // namespace

TEST_CASE("the jungle melon and vine features match the frozen kotlin") {
    SUBCASE("melonNormal") {
        const auto [melons, vines, distinct, checksum] =
            run(60, [](Chunk& chunk, basalt::server::random::RandomSource& random) {
                jungleMelonDecoration::normal()(chunk, random);
            });
        CHECK(melons == 171);
        CHECK(vines == 0);
        CHECK(distinct == 0);
        CHECK(checksum == 6591963276072868100LL);
    }

    SUBCASE("melonSparse") {
        const auto [melons, vines, distinct, checksum] =
            run(400, [](Chunk& chunk, basalt::server::random::RandomSource& random) {
                jungleMelonDecoration::sparse()(chunk, random);
            });
        // A far wider sample than the others: this spec is 1/64, and at sixty chunks it never
        // fired at all, so the case measured only its filter rejecting.
        CHECK(melons == 72);
        CHECK(vines == 0);
        CHECK(distinct == 0);
        CHECK(checksum == 3075551533963136675LL);
    }

    SUBCASE("vines") {
        const auto [melons, vines, distinct, checksum] =
            run(60, [](Chunk& chunk, basalt::server::random::RandomSource& random) {
                jungleVineDecoration::decoration()(chunk, random);
            });
        CHECK(melons == 0);
        CHECK(vines == 1116);
        CHECK(distinct == 4);
        CHECK(checksum == 6628475556098057707LL);
    }
}

TEST_CASE("each vine face picks the state the frozen kotlin picks") {
    // One support block per side in turn, so the five packed states and the order the directions are
    // tried are both pinned. The sixth case has no support at all and must place nothing.
    std::string states;
    for (std::int32_t face = 0; face < 6; ++face) {
        Chunk chunk{0, 0};
        const std::int32_t dx = face == 3 ? -1 : (face == 4 ? 1 : 0);
        const std::int32_t dy = face == 0 ? 1 : 0;
        const std::int32_t dz = face == 1 ? -1 : (face == 2 ? 1 : 0);
        if (face != 5) {
            chunk.setBlockStateId(8 + dx, 80 + dy, 8 + dz, registry::blocks::stone().stateId);
        }
        const bool ok = jungleVineDecoration::placeAt(chunk, DecorationPos{8, 80, 8});
        states.append(ok ? "true" : "false").push_back('/');
        states.append(std::to_string(chunk.blockStateId(8, 80, 8))).push_back(',');
    }
    CHECK(states == "true/10121,true/10115,true/10119,true/10122,true/10107,false/0,");
}
