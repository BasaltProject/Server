#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/MelonPatchDecoration.hpp>

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

/// Grass floor with a stripe of soft cover above it: a melon replaces more than air, so a chunk of
/// bare air would never exercise the replaceable list at all.
[[nodiscard]] Chunk jungleChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf("grass_block"));
            if (z % 4 == 0) {
                chunk.setBlockStateId(x, 71, z, stateIdOf("short_grass"));
            }
            if (z % 4 == 1) {
                chunk.setBlockStateId(x, 71, z, stateIdOf("fern"));
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t melons = 0;
    for (std::int32_t y = 69; y <= 76; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id == stateIdOf("melon")) {
                    ++melons;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), melons};
}

[[nodiscard]] std::tuple<std::int32_t, std::size_t, std::int64_t>
run(std::string_view biome, const std::function<void(Chunk&, const testsupport::FixedBiomeEnvironment&)>& body) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t melons = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 200; ++index) {
        Chunk chunk = jungleChunk(index * 3 - 90, index * -7 + 30);
        const testsupport::FixedBiomeEnvironment environment{std::string{biome}};
        body(chunk, environment);
        const auto [chunkChecksum, chunkMelons] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        melons += chunkMelons;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    return {melons, deferred, static_cast<std::int64_t>(checksum)};
}

} // namespace

TEST_CASE("the melon patches place what the frozen kotlin places") {
    SUBCASE("jungle") {
        const auto [melons, deferred, checksum] =
            run("minecraft:jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                melonPatchDecoration::decorate(chunk, 246810LL, environment);
            });
        CHECK(melons == 386);
        CHECK(deferred == 0);
        CHECK(checksum == 6943304616990820744LL);
    }

    SUBCASE("bambooJungle") {
        const auto [melons, deferred, checksum] =
            run("minecraft:bamboo_jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                melonPatchDecoration::decorate(chunk, 246810LL, environment);
            });
        CHECK(melons == 386);
        CHECK(deferred == 0);
        CHECK(checksum == 6943304616990820744LL);
    }

    SUBCASE("sparseJungle") {
        const auto [melons, deferred, checksum] =
            run("minecraft:sparse_jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                melonPatchDecoration::decorate(chunk, 246810LL, environment);
            });
        CHECK(melons == 59);
        CHECK(deferred == 0);
        CHECK(checksum == -440752667340172660LL);
    }

    SUBCASE("wrong") {
        // plains is in neither spec's list, so this is the real rejection case.
        const auto [melons, deferred, checksum] =
            run("minecraft:plains", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                melonPatchDecoration::decorate(chunk, 246810LL, environment);
            });
        CHECK(melons == 0);
        CHECK(deferred == 0);
        CHECK(checksum == -6307630758989541683LL);
    }

    SUBCASE("sparseOnly") {
        const auto [melons, deferred, checksum] =
            run("minecraft:sparse_jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                melonPatchDecoration::decorateFeature(chunk, 246810LL, environment, "patch_melon_sparse");
            });
        CHECK(melons == 59);
        CHECK(deferred == 0);
        CHECK(checksum == -440752667340172660LL);
    }

    SUBCASE("regional") {
        const auto [melons, deferred, checksum] =
            run("minecraft:jungle", [](Chunk& chunk, const testsupport::FixedBiomeEnvironment& environment) {
                melonPatchDecoration::decorate(
                    chunk, 246810LL, environment,
                    [](std::int32_t, std::int32_t y, std::int32_t) -> std::optional<std::int32_t> {
                        if (y >= 66 && y <= 69) {
                            return registry::blocks::stone().stateId;
                        }
                        return y == 70 ? stateIdOf("grass_block") : registry::blocks::air().stateId;
                    });
            });
        CHECK(melons == 386);
        CHECK(deferred == 158);
        CHECK(checksum == 6943304616990820744LL);
    }
}

TEST_CASE("one melon patch at an origin matches the frozen kotlin") {
    Chunk chunk = jungleChunk(0, 0);
    basalt::server::random::RandomSource random =
        basalt::server::random::RandomSource::worldgen(basalt::server::random::RandomSource::legacy(112233LL));
    melonPatchDecoration::placeAt(chunk, random, DecorationPos{8, 71, 8});
    CHECK(fold(chunk).second == 21);
    CHECK(fold(chunk).first == -5271673066369817519LL);
    CHECK(random.nextInt() == 1397497237);
}
