#include "NetherVegetalShared.hpp"

#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>

#include <array>
#include <cstdint>
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

/// parity: the two vines' `weighted_list` plant height: 10 for the short range, 2 for the long one
/// and 3 for the fixed height, which draws nothing.
[[nodiscard]] std::int32_t vinePlantHeight(random::RandomSource& random, std::int32_t shortMin, std::int32_t shortMax,
                                           std::int32_t longMin, std::int32_t longMax, std::int32_t fixed) {
    const std::int32_t pick = random.nextInt(15);
    if (pick < 10) {
        return random.nextInt(shortMax - shortMin + 1) + shortMin;
    }
    if (pick < 12) {
        return random.nextInt(longMax - longMin + 1) + longMin;
    }
    return fixed;
}

/// parity: `BlockColumnFeature` with a plant layer and a one-block tip, `prioritize_tip` on: the room
/// is checked from the block after the origin, and a short column loses plant blocks first.
void placeVineColumn(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                     std::int32_t step, std::int32_t plantHeight, std::string_view plant, std::string_view tip) {
    std::int32_t plants = plantHeight;
    std::int32_t tips = 1;
    const std::int32_t total = plants + tips;
    for (std::int32_t index = 0; index < total; ++index) {
        if (!isAir(chunk, x, y + (index + 1) * step, z)) {
            std::int32_t remove = total - index;
            const std::int32_t fromPlants = std::min(plants, remove);
            plants -= fromPlants;
            remove -= fromPlants;
            tips -= std::min(tips, remove);
            break;
        }
    }
    std::int32_t at = y;
    for (std::int32_t index = 0; index < plants; ++index) {
        setBlock(chunk, x, at, z, stateId(plant));
        at += step;
    }
    if (tips > 0) {
        setBlock(chunk, x, at, z, vineHeadState(tip, random));
    }
}

} // namespace

/// parity: the 26.3 `twisting_vines` placement after its first filter: `count` 64, a uniform offset,
/// a scan down to the ground and the same filter again, then the column.
void placeTwistingVinesFeature(Chunk& chunk, random::RandomSource& random, const Pos& origin) {
    if (isInvalidTwistingPosition(chunk, origin.x, origin.y, origin.z)) {
        return;
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t attempt = 0; attempt < 64; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(17) - 8;
        std::int32_t y = origin.y + random.nextInt(9) - 4 - 1;
        const std::int32_t z = origin.z + random.nextInt(17) - 8;
        // parity: `environment_scan` down for up to 32 steps to the first block that is not air.
        bool found = false;
        for (std::int32_t step = 0; step <= 32; ++step) {
            if (!isAir(chunk, x, y, z)) {
                found = true;
                break;
            }
            if (step == 32) {
                break;
            }
            --y;
            if (y < bounds.minY() || y > bounds.maxY()) {
                break;
            }
        }
        if (!found) {
            continue;
        }
        ++y;
        if (isInvalidTwistingPosition(chunk, x, y, z)) {
            continue;
        }
        const std::int32_t plants = vinePlantHeight(random, 0, 7, 1, 15, 0);
        placeVineColumn(chunk, random, x, y, z, 1, plants, "twisting_vines_plant", "twisting_vines");
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

/// parity: the 26.3 `weeping_vines` `overlay`: a `random_neighbor_spread` of nether wart around the
/// origin, then a hundred columns hung from the roof.
void placeWeepingVinesFeature(Chunk& chunk, random::RandomSource& random, const Pos& origin) {
    if (!isAir(chunk, origin.x, origin.y, origin.z)) {
        return;
    }
    if (!weepingRoofSupport().contains(blockPathAt(chunk, origin.x, origin.y + 1, origin.z))) {
        return;
    }
    // parity: the y offset is `trapezoid` -4..1 with a plateau of 3: -4 plus 0..4 plus 0..1.
    const auto yOffset = [&random] { return random.nextInt(5) + random.nextInt(2) - 4; };
    setBlock(chunk, origin.x, origin.y, origin.z, stateId("nether_wart_block"));
    for (std::int32_t attempt = 0; attempt < 200; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(6) - random.nextInt(6);
        const std::int32_t y = origin.y + yOffset();
        const std::int32_t z = origin.z + random.nextInt(6) - random.nextInt(6);
        if (!isAir(chunk, x, y, z)) {
            continue;
        }
        std::int32_t neighbours = 0;
        for (const std::array<std::int32_t, 3>& offset : kAllNeighbours) {
            if (weepingRoofSupport().contains(blockPathAt(chunk, x + offset[0], y + offset[1], z + offset[2]))) {
                ++neighbours;
            }
            if (neighbours > 1) {
                break;
            }
        }
        if (neighbours == 1) {
            setBlock(chunk, x, y, z, stateId("nether_wart_block"));
        }
    }
    for (std::int32_t attempt = 0; attempt < 100; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(8) - random.nextInt(8);
        const std::int32_t y = origin.y + yOffset();
        const std::int32_t z = origin.z + random.nextInt(8) - random.nextInt(8);
        if (!isAir(chunk, x, y, z) || !weepingRoofSupport().contains(blockPathAt(chunk, x, y + 1, z))) {
            continue;
        }
        const std::int32_t plants = vinePlantHeight(random, 1, 8, 2, 17, 1);
        placeVineColumn(chunk, random, x, y, z, -1, plants, "weeping_vines_plant", "weeping_vines");
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
