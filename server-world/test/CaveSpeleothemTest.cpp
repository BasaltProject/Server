#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/CaveSpeleothemDecoration.hpp>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::FixedBiomeEnvironment;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

constexpr std::array<std::int32_t, 5> kCavernBottoms{-40, -10, 20, 50, 80};
constexpr std::int32_t kCavernHeight = 8;

/// Stone with flat caverns through it, and sulfur lining half of them: the sulfur spikes only grow
/// from sulfur or cinnabar, so a chunk of plain stone would place nothing at all.
[[nodiscard]] Chunk caveChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -64; y < 120; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            for (const std::int32_t bottom : kCavernBottoms) {
                for (std::int32_t y = bottom; y < bottom + kCavernHeight; ++y) {
                    chunk.setBlockStateId(x, y, z, registry::blocks::air().stateId);
                }
                if ((x + z) % 2 != 0) {
                    continue;
                }
                for (std::int32_t offset = 1; offset <= 3; ++offset) {
                    chunk.setBlockStateId(x, bottom - offset, z, stateIdOf("sulfur"));
                    chunk.setBlockStateId(x, bottom + kCavernHeight - 1 + offset, z, stateIdOf("cinnabar"));
                }
            }
        }
    }
    return chunk;
}

/// The same terrain read from outside the chunk, so a cluster reaching past its edge lands.
[[nodiscard]] std::optional<std::int32_t> neighbourTerrain(std::int32_t x, std::int32_t y, std::int32_t z) {
    if (y < -64 || y >= 120) {
        return std::nullopt;
    }
    for (const std::int32_t bottom : kCavernBottoms) {
        if (y >= bottom && y < bottom + kCavernHeight) {
            return registry::blocks::air().stateId;
        }
        if ((x + z) % 2 != 0) {
            continue;
        }
        if (y >= bottom - 3 && y < bottom) {
            return stateIdOf("sulfur");
        }
        if (y >= bottom + kCavernHeight && y < bottom + kCavernHeight + 3) {
            return stateIdOf("cinnabar");
        }
    }
    return registry::blocks::stone().stateId;
}

[[nodiscard]] std::pair<std::int64_t, std::int32_t> fold(const Chunk& chunk) {
    static const std::set<std::string, std::less<>> counted = {"pointed_dripstone", "dripstone_block", "sulfur_spike",
                                                               "water"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    for (std::int32_t y = -64; y < 120; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && counted.contains(block->path())) {
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
};

[[nodiscard]] Run run(std::string_view biome, std::int32_t index, bool outside) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    const FixedBiomeEnvironment environment{std::string{biome}};
    const ExternalStateReader external = outside ? neighbourTerrain : nullptr;
    for (std::int32_t chunkIndex = 0; chunkIndex < 4; ++chunkIndex) {
        Chunk chunk = caveChunk(chunkIndex * 3 - 7, chunkIndex * -2 + 5);
        if (index < 0) {
            caveSpeleothemDecoration::decorate(chunk, 8821LL + chunkIndex, environment, external);
        } else {
            caveSpeleothemDecoration::decorateFeatures(chunk, 8821LL + chunkIndex, environment, index, index, external);
        }
        const auto [chunkChecksum, chunkPlaced] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.placed += chunkPlaced;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the speleothem steps place what the frozen kotlin places") {
    SUBCASE("dripstoneStep") {
        const Run result = run("minecraft:dripstone_caves", -1, false);
        CHECK(result.placed == 5476);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -3863664998296467291LL);
    }

    SUBCASE("sulfurStep") {
        const Run result = run("minecraft:sulfur_caves", -1, false);
        CHECK(result.placed == 881);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -303786134237502133LL);
    }

    SUBCASE("plainsStep") {
        const Run result = run("minecraft:plains", -1, false);
        CHECK(result.placed == 0);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -8299418820750625711LL);
    }

    SUBCASE("dripstoneOutside") {
        const Run result = run("minecraft:dripstone_caves", -1, true);
        CHECK(result.placed == 7723);
        CHECK(result.deferred == 3393);
        CHECK(result.checksum == 6104574899761635364LL);
    }

    SUBCASE("sulfurOutside") {
        const Run result = run("minecraft:sulfur_caves", -1, true);
        CHECK(result.placed == 1030);
        CHECK(result.deferred == 1405);
        CHECK(result.checksum == 2073355982096758970LL);
    }

    SUBCASE("index_2") {
        const Run result = run("minecraft:sulfur_caves", 2, false);
        CHECK(result.placed == 729);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 5767671560785310935LL);
    }

    SUBCASE("index_3") {
        const Run result = run("minecraft:sulfur_caves", 3, false);
        CHECK(result.placed == 150);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 5792723947235717594LL);
    }

    SUBCASE("index_4") {
        const Run result = run("minecraft:dripstone_caves", 4, false);
        CHECK(result.placed == 5039);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == -2554401543109662496LL);
    }

    SUBCASE("index_5") {
        const Run result = run("minecraft:dripstone_caves", 5, false);
        CHECK(result.placed == 551);
        CHECK(result.deferred == 0);
        CHECK(result.checksum == 4011327289377437954LL);
    }
}

TEST_CASE("one speleothem feature at one position matches the frozen kotlin") {
    constexpr std::array<std::string_view, 5> kFeatures{"dripstone_cluster", "pointed_dripstone", "sulfur_spike",
                                                        "sulfur_spike_cluster", "not_a_feature"};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t placed = 0;
    std::string known;
    std::int32_t after = 0;
    for (const std::string_view feature : kFeatures) {
        for (std::int32_t chunkIndex = 0; chunkIndex < 3; ++chunkIndex) {
            Chunk chunk = caveChunk(chunkIndex, -chunkIndex);
            rng::RandomSource random =
                rng::RandomSource::worldgen(rng::RandomSource::legacy(977LL + chunkIndex * 4441LL));
            // The lining alternates by column, so walking z picks a sulfur column and a stone one in
            // turn: one origin for every feature would leave two of them with no base.
            const DecorationPos origin{
                .x = chunk.chunkX() * 16 + 8, .y = kCavernBottoms[1], .z = chunk.chunkZ() * 16 + 8 + chunkIndex};
            known += caveSpeleothemDecoration::placeAt(chunk, random, origin, feature) ? "1" : "0";
            const auto [chunkChecksum, chunkPlaced] = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
            placed += chunkPlaced;
            after = random.nextInt();
        }
    }
    CHECK(known == "111010101111000");
    CHECK(placed == 1164);
    CHECK(static_cast<std::int64_t>(checksum) == 1590976470338268781LL);
    CHECK(after == -553721973);
}
