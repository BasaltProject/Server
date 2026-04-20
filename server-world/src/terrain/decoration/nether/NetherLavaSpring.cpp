#include "NetherVegetalShared.hpp"

#include <basalt/server/registry/Blocks.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace basalt::server::world::terrain::decoration::nether::shared {
namespace {

const PathSet& allBiomes() {
    static const PathSet paths = {"nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest",
                                  "basalt_deltas"};
    return paths;
}

/// parity: the four main nether biomes list the overworld's `spring_lava`.
const PathSet& springRocks() {
    static const PathSet paths = {"stone", "granite", "diorite", "andesite", "deepslate", "tuff", "calcite", "dirt"};
    return paths;
}

constexpr std::array<std::array<std::int32_t, 3>, 5> kSpringNeighbours{
    std::array<std::int32_t, 3>{-1, 0, 0}, std::array<std::int32_t, 3>{1, 0, 0}, std::array<std::int32_t, 3>{0, 0, -1},
    std::array<std::int32_t, 3>{0, 0, 1}, std::array<std::int32_t, 3>{0, -1, 0}};

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] bool isSpringRock(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = terrainStateAt(chunk, x, y, z);
    return state.has_value() && springRocks().contains(blockPath(*state));
}

/// parity: `VeryBiasedToBottomInt(8, 120, 8)`, which draws three times to bias low.
[[nodiscard]] std::int32_t veryBiasedHeight(random::RandomSource& random) {
    const std::int32_t upperInclusive = 8 + random.nextInt(112);
    const std::int32_t biasedUpperInclusive = random.nextInt(upperInclusive);
    return random.nextInt(biasedUpperInclusive + 8);
}

} // namespace

void placeLavaSpring(Chunk& chunk, const std::int32_t x, const std::int32_t y, const std::int32_t z) {
    if (!isSpringRock(chunk, x, y + 1, z) || !isSpringRock(chunk, x, y - 1, z)) {
        return;
    }
    const std::optional<std::int32_t> current = terrainStateAt(chunk, x, y, z);
    if (!current.has_value()) {
        return;
    }
    if (!registry::blocks::isAir(*current) && !isSpringRock(chunk, x, y, z)) {
        return;
    }
    std::int32_t rocks = 0;
    std::int32_t holes = 0;
    for (const std::array<std::int32_t, 3>& offset : kSpringNeighbours) {
        const std::optional<std::int32_t> state = terrainStateAt(chunk, x + offset[0], y + offset[1], z + offset[2]);
        if (!state.has_value()) {
            continue;
        }
        if (springRocks().contains(blockPath(*state))) {
            ++rocks;
        }
        if (registry::blocks::isAir(*state)) {
            ++holes;
        }
    }
    if (rocks == 4 && holes == 1) {
        setBlock(chunk, x, y, z, registry::blocks::named("lava")->stateId);
        // parity: `SpringFeature` schedules the fluid when it places it.
        chunk.scheduleFluidTick(ChunkScheduledTick{.type = wire::common::Identifier::vanilla("lava"),
                                                   .localX = x & 15,
                                                   .y = y,
                                                   .localZ = z & 15,
                                                   .delay = 0,
                                                   .priority = 0});
    }
}

void placeLavaSprings(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource) {
    for (std::int32_t attempt = 0; attempt < 20; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = veryBiasedHeight(random);
        if (allBiomes().contains(biomePath(biomeSource(x, y, z, nullptr)))) {
            placeLavaSpring(chunk, x, y, z);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::nether::shared
