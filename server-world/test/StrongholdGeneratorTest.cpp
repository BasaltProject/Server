#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/StrongholdStructureGenerator.hpp>

#include <doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <tuple>

using namespace basalt::server::world;
using namespace basalt::server::world::terrain;
namespace registry = basalt::server::registry;

namespace {

[[nodiscard]] Chunk stoneChunk(std::int32_t chunkX, std::int32_t chunkZ) {
    Chunk chunk{chunkX, chunkZ};
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t y = -32; y <= 70; ++y) {
                chunk.setBlockStateId(x, y, z, registry::blocks::stone().stateId);
            }
        }
    }
    return chunk;
}

[[nodiscard]] std::tuple<std::int64_t, std::int32_t, std::size_t> fold(const Chunk& chunk) {
    std::uint64_t checksum = 14695981039346656037ULL;
    std::int32_t written = 0;
    for (std::int32_t y = -32; y <= 74; ++y) {
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
    std::size_t starts = 0;
    std::string located;
    std::int64_t checksum = 0;
};

[[nodiscard]] Run run(std::int64_t seed, std::string_view biome, std::int32_t chunks) {
    const std::string biomeName{biome};
    // Sixteen rings crowded into the same few chunks, so a chunk can see two starts at once and the
    // generator's "seed the feature random only for the first" rule becomes observable.
    structure::ConcentricRingsPlacement rings{
        2, 1, 16, {biomeName}, [biomeName](std::int32_t, std::int32_t, std::int32_t) -> std::string_view {
            return biomeName;
        }};
    // Banded on the quart coordinate, so which coordinate the validity probe is handed shows.
    const auto probe = [biomeName](std::int32_t quartX, std::int32_t, std::int32_t) {
        const std::int32_t band = quartX >= 0 ? quartX / 64 : (quartX - 63) / 64;
        return band % 2 == 0 ? biomeName : std::string{"minecraft:the_void"};
    };
    StrongholdStructureGenerator generator{rings, seed, probe, {biomeName}};
    // Centred on the ring the world actually placed, so the window sees the maze at all.
    const std::optional<std::pair<std::int32_t, std::int32_t>> centre = generator.locate(0, 0);
    REQUIRE(centre.has_value());
    Run result;
    std::uint64_t checksum = 14695981039346656037ULL;
    for (std::int32_t index = 0; index < chunks; ++index) {
        Chunk chunk = stoneChunk(centre->first + index % 10 - 5, centre->second + index / 10 - 5);
        generator.generateInto(chunk, seed);
        const auto [chunkChecksum, written, blockEntities] = fold(chunk);
        checksum = (checksum ^ static_cast<std::uint64_t>(chunkChecksum)) * 1099511628211ULL;
        result.written += written;
        result.blockEntities += blockEntities;
        result.starts += chunk.structures().starts().size();
    }
    result.located = std::to_string(centre->first) + "," + std::to_string(centre->second);
    result.checksum = static_cast<std::int64_t>(checksum);
    return result;
}

void check(const Run& result, std::int32_t written, std::size_t blockEntities, std::size_t starts,
           std::string_view located, std::int64_t checksum) {
    CHECK(result.written == written);
    CHECK(result.blockEntities == blockEntities);
    CHECK(result.starts == starts);
    CHECK(result.located == located);
    CHECK(result.checksum == checksum);
}

} // namespace

TEST_CASE("the strongholds sit where the frozen kotlin puts them") {
    SUBCASE("plains") { check(run(4242, "minecraft:plains", 100), 29475, 9, 39, "8,-1", -2617520323831189796LL); }

    SUBCASE("otherSeed") { check(run(99, "minecraft:plains", 100), 34306, 11, 41, "6,-2", -405377356648808888LL); }
}
