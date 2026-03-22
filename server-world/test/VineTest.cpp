#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/VinePatchDecoration.hpp>

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

/// Stone pillars with air between them across the y range the feature scatters into. A vine needs a
/// sturdy face on one of five sides, so open air alone would place nothing and a solid block would
/// leave no air to place into: the pillars give both, and different faces at different positions,
/// which is what makes the direction order observable.
[[nodiscard]] Chunk pillarChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 60; y < 64; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            if ((x % 4 == 0 && z % 3 == 0) || (x % 5 == 2 && z % 4 == 1)) {
                for (std::int32_t y = 64; y <= 100; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
                }
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t vines = 0;
    for (std::int32_t y = 60; y <= 101; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (registry::blockStateFamilies::isVineState(id)) {
                    ++vines;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), vines};
}

[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t vines = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 60; ++index) {
        Chunk chunk = pillarChunk(index * 3 - 30, index * -5 + 15);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkVines] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        vines += chunkVines;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {vines, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the jungle vine feature places what the frozen kotlin places") {
    SUBCASE("jungle") {
        const auto [vines, deferred, checksum] =
            run("minecraft:jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                vinePatchDecoration::decorate(chunk, 191919LL, environment);
            });
        CHECK(vines == 3179);
        CHECK(deferred == 0);
        CHECK(checksum == -5938702931856289325LL);
    }

    SUBCASE("bamboo") {
        const auto [vines, deferred, checksum] =
            run("minecraft:bamboo_jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                vinePatchDecoration::decorate(chunk, 191919LL, environment);
            });
        CHECK(vines == 3179);
        CHECK(deferred == 0);
        CHECK(checksum == -5938702931856289325LL);
    }

    SUBCASE("sparse") {
        const auto [vines, deferred, checksum] =
            run("minecraft:sparse_jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                vinePatchDecoration::decorate(chunk, 191919LL, environment);
            });
        CHECK(vines == 3179);
        CHECK(deferred == 0);
        CHECK(checksum == -5938702931856289325LL);
    }

    SUBCASE("wrong") {
        // plains is not one of the three jungle biomes, so this is the real rejection case.
        const auto [vines, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                vinePatchDecoration::decorate(chunk, 191919LL, environment);
            });
        CHECK(vines == 0);
        CHECK(deferred == 0);
        CHECK(checksum == 6245282834932346581LL);
    }

    SUBCASE("regional") {
        // Nothing defers here: a vine only ever writes inside its own chunk. What the reader
        // changes is the neighbour lookups at the chunk edge, which is why the count moves.
        const auto [vines, deferred, checksum] =
            run("minecraft:jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                vinePatchDecoration::decorate(
                    chunk, 191919LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        return y >= 60 && y <= 100 ? registry::blocks::stone().stateId
                                                   : registry::blocks::air().stateId;
                    });
            });
        CHECK(vines == 3886);
        CHECK(deferred == 0);
        CHECK(checksum == -1801107157960017159LL);
    }
}

TEST_CASE("each attachment face picks the state the frozen kotlin picks") {
    // Each side on its own, so the five states and the direction priority are pinned directly.
    std::string states;
    for (std::int32_t face = 0; face < 6; ++face) {
        Chunk chunk{0, 0};
        const std::int32_t dx = face == 2 ? 1 : (face == 3 ? -1 : 0);
        const std::int32_t dy = face == 0 ? 1 : 0;
        const std::int32_t dz = face == 4 ? 1 : (face == 5 ? -1 : 0);
        if (face != 1) {
            chunk.setBlockStateId(8 + dx, 70 + dy, 8 + dz, registry::blocks::stone().stateId);
        }
        vinePatchDecoration::placeCaveAt(chunk, DecorationPos{8, 70, 8});
        states.append(std::to_string(chunk.blockStateId(8, 70, 8))).push_back(',');
    }
    CHECK(states == "10121,0,10107,10122,10119,10115,");
}
