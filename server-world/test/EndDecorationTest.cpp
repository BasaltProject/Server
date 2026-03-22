#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"
#include "support/SyntheticRouter.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/densityfunction/DensityFunction.hpp>
#include <basalt/server/world/terrain/NoiseRouterData.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using namespace basalt::server::world::terrain::decoration::end;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] NoiseRouter router() { return basalt::server::world::testsupport::endRouter(0x5DEECE66DLL); }

/// An end stone shelf with open air above it, which is where a chorus plant takes root.
[[nodiscard]] Chunk endChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 40; y <= 56 + (x + z) % 3; ++y) {
                chunk.setBlockStateId(x, y, z, stateIdOf("end_stone"));
            }
        }
    }
    return chunk;
}

struct Fold {
    std::int64_t checksum = 0;
    std::int32_t chorus = 0;
    std::int32_t endStone = 0;
};

[[nodiscard]] Fold fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Fold result;
    for (std::int32_t y = 30; y <= 100; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr) {
                    continue;
                }
                if (block->path() == "chorus_plant" || block->path() == "chorus_flower") {
                    ++result.chorus;
                } else if (block->path() == "end_stone") {
                    ++result.endStone;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the end raw generation step matches the frozen kotlin") {
    const TheEndBiomeSource biomeSource{router()};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t endStone = 0;
    std::size_t deferred = 0;
    for (std::int32_t index = 0; index < 40; ++index) {
        Chunk chunk{index * 37 + 400, index * -53 - 300};
        endRawDecoration::decorate(chunk, 909LL + index, biomeSource);
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        endStone += folded.endStone;
        deferred += chunk.takeGeneratedBlockWrites().size();
    }
    CHECK(endStone == 639);
    CHECK(deferred == 134);
    CHECK(static_cast<std::int64_t>(checksum) == -2350987715176942794LL);
}

TEST_CASE("one end island matches the frozen kotlin") {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t endStone = 0;
    std::int32_t after = 0;
    for (std::int32_t index = 0; index < 12; ++index) {
        Chunk chunk{index, -index};
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(77LL + index * 911LL));
        endRawDecoration::placeAt(chunk, random, index * 16 + 8, 60, -index * 16 + 8);
        const Fold folded = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
        endStone += folded.endStone;
        after = random.nextInt();
    }
    CHECK(endStone == 4005);
    CHECK(static_cast<std::int64_t>(checksum) == 1274422041366887508LL);
    CHECK(after == 1395026466);
}

TEST_CASE("the end platform matches the frozen kotlin") {
    const TheEndBiomeSource biomeSource{router()};
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t obsidian = 0;
    for (std::int32_t chunkX = 5; chunkX <= 7; ++chunkX) {
        for (std::int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
            Chunk chunk{chunkX, chunkZ};
            for (std::int32_t x = 0; x < 16; ++x) {
                for (std::int32_t z = 0; z < 16; ++z) {
                    for (std::int32_t y = 40; y <= 55; ++y) {
                        chunk.setBlockStateId(x, y, z, stateIdOf("end_stone"));
                    }
                }
            }
            endTopLayerDecoration::decorate(chunk, biomeSource);
            for (std::int32_t y = 30; y <= 60; ++y) {
                for (std::int32_t z = 0; z < 16; ++z) {
                    for (std::int32_t x = 0; x < 16; ++x) {
                        const std::int32_t id = chunk.blockStateId(x, y, z);
                        checksum =
                            (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                        const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                        if (block != nullptr && block->path() == "obsidian") {
                            ++obsidian;
                        }
                    }
                }
            }
        }
    }
    CHECK(obsidian == 25);
    CHECK(static_cast<std::int64_t>(checksum) == 7994176547612088004LL);
}

TEST_CASE("the end chorus plants match the frozen kotlin") {
    const TheEndBiomeSource biomeSource{router()};

    SUBCASE("step") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t chorus = 0;
        std::size_t deferred = 0;
        for (std::int32_t index = 0; index < 24; ++index) {
            Chunk chunk = endChunk(index * 37 + 400, index * -53 - 300);
            endVegetalDecoration::decorate(chunk, 4242LL + index, router(), biomeSource);
            const Fold folded = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
            chorus += folded.chorus;
            deferred += chunk.takeGeneratedBlockWrites().size();
        }
        CHECK(chorus == 104);
        CHECK(deferred == 0);
        CHECK(static_cast<std::int64_t>(checksum) == 1436036048102344832LL);
    }

    SUBCASE("at") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t chorus = 0;
        std::string placed;
        std::int32_t after = 0;
        for (std::int32_t index = 0; index < 12; ++index) {
            Chunk chunk = endChunk(index * 37 + 400, index * -53 - 300);
            rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(1234LL + index * 7717LL));
            const std::int32_t x = chunk.chunkX() * 16 + 8;
            const std::int32_t z = chunk.chunkZ() * 16 + 8;
            const std::int32_t y = chunk.heightAt(8, 8, HeightmapType::MotionBlocking);
            placed += endVegetalDecoration::placeAt(chunk, random, router(), x, y, z) ? "1" : "0";
            const Fold folded = fold(chunk);
            checksum = (checksum ^ static_cast<std::uint64_t>(folded.checksum)) * 1099511628211ULL;
            chorus += folded.chorus;
            after = random.nextInt();
        }
        CHECK(placed == "111111111111");
        CHECK(chorus == 323);
        CHECK(static_cast<std::int64_t>(checksum) == 6086701434972160716LL);
        CHECK(after == 1587146041);
    }
}

TEST_CASE("the end spikes match the frozen kotlin") {
    SUBCASE("spikes") {
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t obsidian = 0;
        std::size_t entities = 0;
        std::string hit;
        for (std::int32_t chunkX = -4; chunkX <= 4; ++chunkX) {
            for (std::int32_t chunkZ = -4; chunkZ <= 4; ++chunkZ) {
                Chunk chunk{chunkX, chunkZ, kEndHeight};
                hit += endSurfaceStructuresDecoration::placeSpikesIn(chunk, 987654321LL) ? "1" : "0";
                for (std::int32_t y = 0; y <= 120; ++y) {
                    for (std::int32_t z = 0; z < 16; ++z) {
                        for (std::int32_t x = 0; x < 16; ++x) {
                            const std::int32_t id = chunk.blockStateId(x, y, z);
                            checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) *
                                       1099511628211ULL;
                            const basalt::wire::common::Identifier* block =
                                registry::blockStateDefinitions::blockOf(id);
                            if (block != nullptr && block->path() == "obsidian") {
                                ++obsidian;
                            }
                        }
                    }
                }
                entities += chunk.generatedEntities().size();
            }
        }
        CHECK(hit == "000000000001111000011001100010000100010000100001001000001111000000000000000000000");
        CHECK(obsidian == 40499);
        CHECK(entities == 10);
        CHECK(static_cast<std::int64_t>(checksum) == 3183473030495876510LL);
    }

    SUBCASE("step") {
        const TheEndBiomeSource biomeSource{router()};
        std::uint64_t checksum = 14695981039346656037ULL;
        std::int32_t obsidian = 0;
        std::size_t entities = 0;
        for (std::int32_t chunkX = -3; chunkX <= 3; ++chunkX) {
            for (std::int32_t chunkZ = -3; chunkZ <= 3; ++chunkZ) {
                Chunk chunk{chunkX, chunkZ, kEndHeight};
                endSurfaceStructuresDecoration::decorate(chunk, 987654321LL, router(), biomeSource);
                for (std::int32_t y = 0; y <= 120; ++y) {
                    for (std::int32_t z = 0; z < 16; ++z) {
                        for (std::int32_t x = 0; x < 16; ++x) {
                            const std::int32_t id = chunk.blockStateId(x, y, z);
                            checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) *
                                       1099511628211ULL;
                            const basalt::wire::common::Identifier* block =
                                registry::blockStateDefinitions::blockOf(id);
                            if (block != nullptr && block->path() == "obsidian") {
                                ++obsidian;
                            }
                        }
                    }
                }
                entities += chunk.generatedEntities().size();
            }
        }
        CHECK(obsidian == 40499);
        CHECK(entities == 10);
        CHECK(static_cast<std::int64_t>(checksum) == -1983311235934239330LL);
    }
}
