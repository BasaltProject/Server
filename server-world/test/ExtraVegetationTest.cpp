#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/ExtraVegetationDecoration.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain::decoration;
using basalt::server::world::testsupport::stateIdOf;
namespace rng = basalt::server::random;
namespace registry = basalt::server::registry;

namespace {

/// A shore around a pond, which is what cane and fireflies need beside them. Only the top layers
/// are ever read, and the pumpkin case needs 900 chunks to fire at all.
[[nodiscard]] Chunk shoreChunk(std::int32_t chunkX, std::int32_t chunkZ, std::string_view surface) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = 66; y < 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
            chunk.setBlockStateId(x, 70, z, stateIdOf(surface));
        }
    }
    for (std::int32_t x = 5; x <= 10; ++x) {
        for (std::int32_t z = 5; z <= 10; ++z) {
            chunk.setBlockStateId(x, 70, z, registry::blocks::water().stateId);
            chunk.setBlockStateId(x, 69, z, stateIdOf("sand"));
        }
    }
    return chunk;
}

struct Counts {
    std::int32_t pumpkins = 0;
    std::int32_t cane = 0;
    std::int32_t cactus = 0;
    std::int32_t bushes = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Counts fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    Counts result;
    for (std::int32_t y = 66; y <= 100; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block == nullptr) {
                    continue;
                }
                const std::string path = block->path();
                if (path == "pumpkin") {
                    ++result.pumpkins;
                } else if (path == "sugar_cane") {
                    ++result.cane;
                } else if (path == "cactus" || path == "cactus_flower") {
                    ++result.cactus;
                } else if (path == "firefly_bush") {
                    ++result.bushes;
                }
            }
        }
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

struct Run {
    Counts totals;
    std::int64_t checksum = 0;
    std::int32_t after = 0;
    /// What landed in a neighbour. These features scatter past the chunk edge, so counting only what
    /// stayed inside would let a lost write pass for a correct one.
    std::size_t deferred = 0;
};

[[nodiscard]] Run run(const BiomeVegetalDecoration& decoration, std::string_view surface, std::int32_t count) {
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < count; ++index) {
        Chunk chunk = shoreChunk(index * 5 - 30, index * -3 + 12, surface);
        rng::RandomSource random = rng::RandomSource::worldgen(rng::RandomSource::legacy(31LL + index * 7919LL));
        decoration(chunk, random);
        const Counts counts = fold(chunk);
        result.totals.pumpkins += counts.pumpkins;
        result.totals.cane += counts.cane;
        result.totals.cactus += counts.cactus;
        result.totals.bushes += counts.bushes;
        checksum = (checksum ^ static_cast<std::uint64_t>(counts.checksum)) * 1099511628211ULL;
        result.deferred += chunk.takeGeneratedBlockWrites().size();
        result.after = random.nextInt();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the shared extra vegetation places what the frozen kotlin places") {
    SUBCASE("pumpkinOnly") {
        const Run result = run(extraVegetationDecoration::pumpkinOnly(), "grass_block", 900);
        CHECK(result.totals.pumpkins == 31);
        CHECK(result.totals.cane == 0);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == 3578738381038222779LL);
        CHECK(result.after == 2055286257);
        CHECK(result.deferred == 0);
    }

    SUBCASE("standard") {
        const Run result = run(extraVegetationDecoration::standard(), "grass_block", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 45);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 30);
        CHECK(result.checksum == -2358077318708963176LL);
        CHECK(result.after == -536750048);
        CHECK(result.deferred == 0);
    }

    SUBCASE("standardWithoutPumpkin") {
        const Run result = run(extraVegetationDecoration::standardWithoutPumpkin(), "grass_block", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 54);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 46);
        CHECK(result.checksum == -8035072488944382251LL);
        CHECK(result.after == 588904721);
        CHECK(result.deferred == 0);
    }

    SUBCASE("fireflyNearWater") {
        const Run result = run(extraVegetationDecoration::fireflyNearWater(), "grass_block", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 0);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 44);
        CHECK(result.checksum == -2154865887007311039LL);
        CHECK(result.after == -276953105);
        CHECK(result.deferred == 0);
    }

    SUBCASE("desert") {
        const Run result = run(extraVegetationDecoration::desert(), "sand", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 256);
        CHECK(result.totals.cactus == 12);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == 8972483978184471983LL);
        CHECK(result.after == -1230455523);
        CHECK(result.deferred == 0);
    }

    SUBCASE("desertWithoutPumpkin") {
        const Run result = run(extraVegetationDecoration::desertWithoutPumpkin(), "sand", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 256);
        CHECK(result.totals.cactus == 23);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == 6686611659611689146LL);
        CHECK(result.after == -813907403);
        CHECK(result.deferred == 0);
    }

    SUBCASE("desertWithoutPumpkinOrCactus") {
        const Run result = run(extraVegetationDecoration::desertWithoutPumpkinOrCactus(), "sand", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 256);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == 7485129438416504129LL);
        CHECK(result.after == -104097446);
        CHECK(result.deferred == 0);
    }

    SUBCASE("badlands") {
        const Run result = run(extraVegetationDecoration::badlands(), "red_sand", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 60);
        CHECK(result.totals.cactus == 8);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == 7715796775260124275LL);
        CHECK(result.after == -831851038);
        CHECK(result.deferred == 0);
    }

    SUBCASE("badlandsWithoutPumpkin") {
        const Run result = run(extraVegetationDecoration::badlandsWithoutPumpkin(), "red_sand", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 60);
        CHECK(result.totals.cactus == 20);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == 8020240781692310841LL);
        CHECK(result.after == -2094109878);
        CHECK(result.deferred == 0);
    }

    SUBCASE("badlandsWithoutPumpkinOrCactus") {
        const Run result = run(extraVegetationDecoration::badlandsWithoutPumpkinOrCactus(), "red_sand", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 60);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == -8055908383301564595LL);
        CHECK(result.after == 588904721);
        CHECK(result.deferred == 0);
    }

    SUBCASE("swamp") {
        const Run result = run(extraVegetationDecoration::swamp(), "grass_block", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 96);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 89);
        CHECK(result.checksum == 6321040793946520093LL);
        CHECK(result.after == 202814787);
        CHECK(result.deferred == 0);
    }

    SUBCASE("swampWithoutPumpkin") {
        const Run result = run(extraVegetationDecoration::swampWithoutPumpkin(), "grass_block", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 96);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 92);
        CHECK(result.checksum == -1005060391652967389LL);
        CHECK(result.after == 203695341);
        CHECK(result.deferred == 0);
    }

    SUBCASE("swampFireflies") {
        const Run result = run(extraVegetationDecoration::swampFireflies(), "grass_block", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 0);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 88);
        CHECK(result.checksum == 1671790454851968293LL);
        CHECK(result.after == -1982035796);
        CHECK(result.deferred == 0);
    }

    SUBCASE("mangrove") {
        const Run result = run(extraVegetationDecoration::mangrove(), "grass_block", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 0);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 44);
        CHECK(result.checksum == -2154865887007311039LL);
        CHECK(result.after == -276953105);
        CHECK(result.deferred == 0);
    }

    SUBCASE("bareStone") {
        const Run result = run(extraVegetationDecoration::standard(), "stone", 60);
        CHECK(result.totals.pumpkins == 0);
        CHECK(result.totals.cane == 0);
        CHECK(result.totals.cactus == 0);
        CHECK(result.totals.bushes == 0);
        CHECK(result.checksum == 1609328623553221673LL);
        CHECK(result.after == -536750048);
        CHECK(result.deferred == 0);
    }
}
