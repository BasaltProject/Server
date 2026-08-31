#include "NetherVegetalShared.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <array>
#include <optional>
#include <stdexcept>
#include <string>

namespace basalt::server::world::terrain::decoration::nether::shared {
namespace {

const PathSet& twistingSupport() {
    static const PathSet paths = {"netherrack", "warped_nylium", "warped_wart_block"};
    return paths;
}

const PathSet& weepingRoofSupport() {
    static const PathSet paths = {"netherrack", "nether_wart_block"};
    return paths;
}

constexpr std::array<std::array<std::int32_t, 3>, 6> kAllNeighbours{
    std::array<std::int32_t, 3>{0, -1, 0}, std::array<std::int32_t, 3>{0, 1, 0},  std::array<std::int32_t, 3>{0, 0, -1},
    std::array<std::int32_t, 3>{0, 0, 1},  std::array<std::int32_t, 3>{-1, 0, 0}, std::array<std::int32_t, 3>{1, 0, 0}};

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

/// parity: the head of a growing vine carries a random age, which decides how much longer it grows.
[[nodiscard]] std::int32_t vineHeadState(std::string_view path, random::RandomSource& random) {
    const std::int32_t age = 17 + random.nextInt(9);
    const std::optional<std::int32_t> head = registry::blockStateFamilies::growingVineHeadStateId(stateId(path), age);
    if (!head.has_value()) {
        throw std::runtime_error{"missing " + std::string{path} + " age " + std::to_string(age)};
    }
    return *head;
}

[[nodiscard]] bool isInvalidTwistingPosition(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    return !isAir(chunk, x, y, z) || !twistingSupport().contains(blockPathAt(chunk, x, y - 1, z));
}

void placeTwistingColumn(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t startY,
                         std::int32_t z, std::int32_t totalHeight) {
    std::int32_t y = startY;
    for (std::int32_t height = 1; height <= totalHeight; ++height) {
        if (isAir(chunk, x, y, z)) {
            if (height == totalHeight || !isAir(chunk, x, y + 1, z)) {
                setBlock(chunk, x, y, z, vineHeadState("twisting_vines", random));
                break;
            }
            setBlock(chunk, x, y, z, stateId("twisting_vines_plant"));
        }
        y++;
    }
}

} // namespace

void placeTwistingVinesFeature(Chunk& chunk, random::RandomSource& random, const Pos& origin) {
    if (isInvalidTwistingPosition(chunk, origin.x, origin.y, origin.z)) {
        return;
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const std::int32_t x = origin.x - 8 + random.nextInt(17);
        std::int32_t y = origin.y - 4 + random.nextInt(9);
        const std::int32_t z = origin.z - 8 + random.nextInt(17);
        bool outOfWorld = false;
        while (true) {
            y--;
            if (y < bounds.minY() || y > bounds.maxY()) {
                outOfWorld = true;
                break;
            }
            if (!isAir(chunk, x, y, z)) {
                break;
            }
        }
        if (outOfWorld) {
            continue;
        }
        y++;
        if (isInvalidTwistingPosition(chunk, x, y, z)) {
            continue;
        }
        std::int32_t vineHeight = 1 + random.nextInt(8);
        if (random.nextInt(6) == 0) {
            vineHeight *= 2;
        }
        if (random.nextInt(5) == 0) {
            vineHeight = 1;
        }
        placeTwistingColumn(chunk, random, x, y, z, vineHeight);
    }
}

void placeTwistingVines(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource) {
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = random.nextInt(128);
        if (biomePath(biomeSource(x, y, z, nullptr)) == "warped_forest") {
            placeTwistingVinesFeature(chunk, random, Pos{.x = x, .y = y, .z = z});
        }
    }
}

void placeWeepingColumn(Chunk& chunk, random::RandomSource& random, const std::int32_t x, const std::int32_t startY,
                        const std::int32_t z, const std::int32_t totalHeight, const std::int32_t minAge) {
    std::int32_t y = startY;
    for (std::int32_t height = 0; height <= totalHeight; ++height) {
        if (isAir(chunk, x, y, z)) {
            if (height == totalHeight || !isAir(chunk, x, y - 1, z)) {
                const std::int32_t age = minAge + random.nextInt(26 - minAge);
                const std::optional<std::int32_t> head =
                    registry::blockStateFamilies::growingVineHeadStateId(stateId("weeping_vines"), age);
                if (!head.has_value()) {
                    throw std::runtime_error{"missing weeping vines age " + std::to_string(age)};
                }
                setBlock(chunk, x, y, z, *head);
                break;
            }
            setBlock(chunk, x, y, z, stateId("weeping_vines_plant"));
        }
        y--;
    }
}

void placeWeepingVinesFeature(Chunk& chunk, random::RandomSource& random, const Pos& origin) {
    if (!isAir(chunk, origin.x, origin.y, origin.z)) {
        return;
    }
    if (!weepingRoofSupport().contains(blockPathAt(chunk, origin.x, origin.y + 1, origin.z))) {
        return;
    }
    setBlock(chunk, origin.x, origin.y, origin.z, stateId("nether_wart_block"));
    for (std::int32_t attempt = 0; attempt < 200; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(6) - random.nextInt(6);
        const std::int32_t y = origin.y + random.nextInt(2) - random.nextInt(5);
        const std::int32_t z = origin.z + random.nextInt(6) - random.nextInt(6);
        if (!isAir(chunk, x, y, z)) {
            continue;
        }
        std::int32_t neighbours = 0;
        for (const std::array<std::int32_t, 3>& offset : kAllNeighbours) {
            if (weepingRoofSupport().contains(blockPathAt(chunk, x + offset[0], y + offset[1], z + offset[2]))) {
                ++neighbours;
            }
        }
        if (neighbours == 1) {
            setBlock(chunk, x, y, z, stateId("nether_wart_block"));
        }
    }
    for (std::int32_t attempt = 0; attempt < 100; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(8) - random.nextInt(8);
        const std::int32_t y = origin.y + random.nextInt(2) - random.nextInt(7);
        const std::int32_t z = origin.z + random.nextInt(8) - random.nextInt(8);
        if (!isAir(chunk, x, y, z) || !weepingRoofSupport().contains(blockPathAt(chunk, x, y + 1, z))) {
            continue;
        }
        std::int32_t vineHeight = 1 + random.nextInt(8);
        if (random.nextInt(6) == 0) {
            vineHeight *= 2;
        }
        if (random.nextInt(5) == 0) {
            vineHeight = 1;
        }
        placeWeepingColumn(chunk, random, x, y, z, vineHeight, 17);
    }
}

void placeWeepingVines(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource) {
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = random.nextInt(128);
        if (biomePath(biomeSource(x, y, z, nullptr)) == "crimson_forest") {
            placeWeepingVinesFeature(chunk, random, Pos{.x = x, .y = y, .z = z});
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::nether::shared
