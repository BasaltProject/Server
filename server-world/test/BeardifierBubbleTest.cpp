#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "support/DecorationFixture.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/densityfunction/SamplerContext.hpp>
#include <basalt/server/world/terrain/BubbleColumnFormation.hpp>
#include <basalt/server/world/terrain/StructureBeardifier.hpp>

#include <doctest.h>

#include <bit>
#include <cstdint>
#include <vector>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
using basalt::server::world::testsupport::stateIdOf;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] std::int32_t floorMod(std::int32_t value, std::int32_t modulus) {
    const std::int32_t rest = value % modulus;
    return rest < 0 ? rest + modulus : rest;
}

struct Run {
    std::int32_t nonZero = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(TerrainAdjustment adjustment) {
    const densityfunction::Sampler beardifier = structureBeardifier(StructureBeard{
        .pieces = {BeardifierRigid{.box = {-4, 60, -4, 4, 70, 4}, .groundLevelDelta = 2, .adjustment = adjustment},
                   BeardifierRigid{.box = {10, 55, 10, 14, 58, 14}, .groundLevelDelta = 0, .adjustment = adjustment}},
        .junctions = {BeardifierJunction{.x = 2, .groundY = 64, .z = -3},
                      BeardifierJunction{.x = 12, .groundY = 57, .z = 12}}});
    densityfunction::SamplerContext context{densityfunction::SamplerContext::Options{}};
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < 2048; ++index) {
        const float value = beardifier->sampleValue(context, floorMod(index * 7, 40) - 12, floorMod(index * 3, 30) + 50,
                                                    floorMod(index * 11, 40) - 12);
        if (value != 0.0F) {
            ++result.nonZero;
        }
        checksum = (checksum ^ std::bit_cast<std::uint32_t>(value)) * 1099511628211ULL;
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

} // namespace

TEST_CASE("the beardifier shapes terrain the same way every time") {
    SUBCASE("NONE") {
        const Run result = run(TerrainAdjustment::None);
        CHECK(result.nonZero == 871);
        CHECK(result.checksum == 7341274215419491589LL);
    }

    SUBCASE("BURY") {
        const Run result = run(TerrainAdjustment::Bury);
        CHECK(result.nonZero == 871);
        CHECK(result.checksum == 8840587058693433711LL);
    }

    SUBCASE("BEARD_THIN") {
        const Run result = run(TerrainAdjustment::BeardThin);
        CHECK(result.nonZero == 1230);
        CHECK(result.checksum == -5044933238332024792LL);
    }

    SUBCASE("BEARD_BOX") {
        const Run result = run(TerrainAdjustment::BeardBox);
        CHECK(result.nonZero == 1331);
        CHECK(result.checksum == 4686971449662833636LL);
    }

    SUBCASE("ENCAPSULATE") {
        const Run result = run(TerrainAdjustment::Encapsulate);
        CHECK(result.nonZero == 1331);
        CHECK(result.checksum == 2458410024933397189LL);
    }
}

TEST_CASE("the bubble columns form the way the frozen kotlin forms them") {
    Chunk chunk{2, -3};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            std::int32_t floor = registry::blocks::stone().stateId;
            switch (floorMod(x * 5 + z, 4)) {
            case 0:
                floor = stateIdOf("soul_sand");
                break;
            case 1:
                floor = stateIdOf("magma_block");
                break;
            case 2:
                floor = stateIdOf("gravel");
                break;
            default:
                break;
            }
            chunk.setBlockStateId(x, 40, z, floor);
            for (std::int32_t y = 41; y <= 48; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::water().stateId);
            }
            // A lid over some columns, so the walk stops before the top of the water.
            if (floorMod(x + z, 3) == 0) {
                chunk.setBlockStateId(x, 45, z, registry::blocks::stone().stateId);
            }
        }
    }
    bubbleColumnFormation::formInto(chunk);
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t columns = 0;
    for (std::int32_t y = 39; y <= 50; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                const basalt::wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(id);
                if (block != nullptr && block->path() == "bubble_column") {
                    ++columns;
                }
            }
        }
    }
    CHECK(columns == 860);
    CHECK(static_cast<std::int64_t>(checksum) == -1118893537038522283LL);
}
