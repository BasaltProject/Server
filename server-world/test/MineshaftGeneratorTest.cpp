#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/MineshaftStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk stoneChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -16; y <= 48; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -16; y <= 52; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                const std::int32_t id = chunk.blockStateId(x, y, z);
                checksum = (checksum ^ static_cast<std::uint64_t>(static_cast<std::int64_t>(id))) * 1099511628211ULL;
                if (id != registry::blocks::stone().stateId && id != registry::blocks::air().stateId) {
                    ++written;
                }
            }
        }
    }
    return {static_cast<std::int64_t>(checksum), written, chunk.blockEntities().size()};
}

struct Run {
    std::int32_t written = 0;
    std::size_t blockEntities = 0;
    std::size_t entities = 0;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::int32_t chunks, MineshaftStructureGenerator::BiomeAt biomeAt) {
    MineshaftStructureGenerator generator{std::move(biomeAt),
                                          // savanna is in both tags, so which type the coin flip drew is what decides.
                                          {"minecraft:plains", "minecraft:forest", "minecraft:savanna"},
                                          {"minecraft:badlands", "minecraft:savanna"}};
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = stoneChunk(index % 8 - 4, index / 8 - 4);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, blockEntities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.blockEntities += blockEntities;
        result.entities += chunk.generatedEntities().size();
    }
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t blockEntities, std::size_t entities,
           std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.entities == entities);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the mineshafts start where the jar starts them") {
    SUBCASE("plains") {
        check(run(12345, 64, [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:plains"}; }),
              6277, 4, 0, -7198636584338907090LL);
    }

    SUBCASE("badlands") {
        check(
            run(12345, 64, [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:badlands"}; }),
            5736, 0, 2, 2318323481289889176LL);
    }

    SUBCASE("nether") {
        check(run(12345, 64,
                  [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:nether_wastes"}; }),
              0, 0, 0, -6177001375394681755LL);
    }

    SUBCASE("both") {
        // A biome in both tags, so the type comes from the coin flip and not from the refusal.
        check(run(12345, 64, [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:savanna"}; }),
              6277, 4, 0, -7198636584338907090LL);
    }

    SUBCASE("layered") {
        // Layered by height, so which y the start probes at is observable.
        check(run(12345, 64,
                  [](std::int32_t, std::int32_t y, std::int32_t) {
                      return std::string{y >= 32 ? "minecraft:plains" : "minecraft:badlands"};
                  }),
              6277, 4, 0, -7198636584338907090LL);
    }

    SUBCASE("otherSeed") {
        check(run(7, 64, [](std::int32_t, std::int32_t, std::int32_t) { return std::string{"minecraft:plains"}; }), 562,
              2, 0, 8726084680966971115LL);
    }
}
