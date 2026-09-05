#include "AquaticSupport.hpp"

#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/AquaticDecoration.hpp>
#include <basalt/server/world/terrain/decoration/CoralDecoration.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace {

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::int32_t localBlock(const Chunk& chunk, std::int32_t worldX, std::int32_t y, std::int32_t worldZ) {
    return chunk.decorationBlockStateId(worldX, y, worldZ);
}

void setLocalBlock(Chunk& chunk, std::int32_t worldX, std::int32_t y, std::int32_t worldZ, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setDecorationBlockStateId(worldX, y, worldZ, stateId);
    }
}

/// The `OCEAN_FLOOR` height of a column. A heightmap belongs to the chunk that owns it, so a column
/// outside is scanned through the same reader the rest of the body uses.
[[nodiscard]] std::int32_t oceanFloorAt(const Chunk& chunk, const std::vector<std::int32_t>& oceanFloor,
                                        std::int32_t worldX, std::int32_t worldZ) {
    if ((worldX >> 4) == chunk.chunkX() && (worldZ >> 4) == chunk.chunkZ()) {
        return oceanFloor[static_cast<std::size_t>((worldX & 15) + (worldZ & 15) * 16)];
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t y = bounds.maxY(); y >= bounds.minY(); --y) {
        if (heightmapMatches(HeightmapType::OceanFloor, chunk.decorationBlockStateId(worldX, y, worldZ))) {
            return y + 1;
        }
    }
    return bounds.minY();
}

[[nodiscard]] bool isWater(const Chunk& chunk, std::int32_t worldX, std::int32_t y, std::int32_t worldZ) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    const wire::common::Identifier* block =
        registry::blockStateDefinitions::blockOf(localBlock(chunk, worldX, y, worldZ));
    return block != nullptr && block->path() == "water";
}

void placeSeagrass(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                   const std::vector<std::int32_t>& oceanFloor, double tallProbability) {
    const std::int32_t x = origin.x + random.nextInt(8) - random.nextInt(8);
    const std::int32_t z = origin.z + random.nextInt(8) - random.nextInt(8);
    const std::int32_t y = oceanFloorAt(chunk, oceanFloor, x, z);
    if (!isWater(chunk, x, y, z)) {
        return;
    }
    const bool tall = random.nextDouble() < tallProbability;
    if (y <= chunk.verticalBounds().minY() || !aquaticsupport::seagrassSupport(localBlock(chunk, x, y - 1, z))) {
        return;
    }
    if (!tall) {
        setLocalBlock(chunk, x, y, z, stateIdOf("seagrass"));
        return;
    }
    if (!isWater(chunk, x, y + 1, z)) {
        return;
    }
    const std::int32_t lower = stateIdOf("tall_seagrass");
    const std::optional<std::int32_t> upper = registry::blockStateFamilies::upperDoublePlantStateId(lower);
    if (!upper.has_value()) {
        throw std::runtime_error{"missing tall seagrass upper half"};
    }
    setLocalBlock(chunk, x, y, z, lower);
    setLocalBlock(chunk, x, y + 1, z, *upper);
}

void placeKelp(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    std::int32_t y = origin.y;
    if (!isWater(chunk, origin.x, y, origin.z)) {
        return;
    }
    const std::int32_t minY = chunk.verticalBounds().minY();
    const std::int32_t kelp = stateIdOf("kelp");
    const std::int32_t kelpPlant = stateIdOf("kelp_plant");
    const std::int32_t height = 1 + random.nextInt(10);
    for (std::int32_t step = 0; step <= height; ++step) {
        const bool canGrow = isWater(chunk, origin.x, y, origin.z) && isWater(chunk, origin.x, y + 1, origin.z) &&
                             y > minY && aquaticsupport::kelpSupport(localBlock(chunk, origin.x, y - 1, origin.z));
        if (!canGrow) {
            if (step > 0 && y - 1 > minY && aquaticsupport::kelpSupport(localBlock(chunk, origin.x, y - 2, origin.z))) {
                setLocalBlock(chunk, origin.x, y - 1, origin.z, kelp + 20 + random.nextInt(4));
            }
            return;
        }
        if (step == height) {
            setLocalBlock(chunk, origin.x, y, origin.z, kelp + 20 + random.nextInt(4));
            return;
        }
        setLocalBlock(chunk, origin.x, y, origin.z, kelpPlant);
        ++y;
    }
}

void placeSeaPickles(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                     const std::vector<std::int32_t>& oceanFloor, std::int32_t count) {
    const std::int32_t minY = chunk.verticalBounds().minY();
    const std::int32_t seaPickle = stateIdOf("sea_pickle");
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = origin.x + random.nextInt(8) - random.nextInt(8);
        const std::int32_t z = origin.z + random.nextInt(8) - random.nextInt(8);
        const std::int32_t y = oceanFloorAt(chunk, oceanFloor, x, z);
        const std::int32_t stateId = seaPickle + random.nextInt(4) * 2;
        if (isWater(chunk, x, y, z) && y > minY && aquaticsupport::seaPickleSupport(localBlock(chunk, x, y - 1, z))) {
            setLocalBlock(chunk, x, y, z, stateId);
        }
    }
}

[[nodiscard]] BiomeVegetalDecoration seagrass(std::int32_t count, double tallProbability) {
    return [count, tallProbability](Chunk& chunk, random::RandomSource& random) {
        const std::vector<std::int32_t> oceanFloor = chunk.heightmap(HeightmapType::OceanFloor);
        const std::vector<PlacementModifier> modifiers = {
            inSquarePlacement(), heightmapPlacement(HeightmapType::OceanFloor), countPlacement(count)};
        const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
        placeFrom(chunk, random, chunkOrigin, modifiers, [&](const DecorationPos& origin) {
            placeSeagrass(chunk, random, origin, oceanFloor, tallProbability);
        });
    };
}

[[nodiscard]] BiomeVegetalDecoration kelp(std::int32_t noiseToCountRatio) {
    return [noiseToCountRatio](Chunk& chunk, random::RandomSource& random) {
        const std::vector<PlacementModifier> modifiers = {noiseBasedCountPlacement(noiseToCountRatio, 80.0),
                                                          inSquarePlacement(),
                                                          heightmapPlacement(HeightmapType::OceanFloor)};
        const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
        placeFrom(chunk, random, chunkOrigin, modifiers,
                  [&](const DecorationPos& origin) { placeKelp(chunk, random, origin); });
    };
}

[[nodiscard]] BiomeVegetalDecoration seaPickles() {
    return [](Chunk& chunk, random::RandomSource& random) {
        const std::vector<std::int32_t> oceanFloor = chunk.heightmap(HeightmapType::OceanFloor);
        const std::vector<PlacementModifier> modifiers = {rarityFilter(16), inSquarePlacement(),
                                                          heightmapPlacement(HeightmapType::OceanFloor)};
        const DecorationPos chunkOrigin{.x = chunk.chunkX() * 16, .y = 0, .z = chunk.chunkZ() * 16};
        placeFrom(chunk, random, chunkOrigin, modifiers,
                  [&](const DecorationPos& origin) { placeSeaPickles(chunk, random, origin, oceanFloor, 20); });
    };
}

} // namespace

BiomeVegetalDecoration aquaticDecoration::ocean() { return combine({seagrass(48, 0.3), kelp(120)}); }

BiomeVegetalDecoration aquaticDecoration::deepOcean() { return combine({seagrass(48, 0.8), kelp(120)}); }

BiomeVegetalDecoration aquaticDecoration::coldOcean() { return combine({seagrass(32, 0.3), kelp(120)}); }

BiomeVegetalDecoration aquaticDecoration::deepColdOcean() { return combine({seagrass(40, 0.8), kelp(120)}); }

BiomeVegetalDecoration aquaticDecoration::lukewarmOcean() { return combine({seagrass(80, 0.3), kelp(80)}); }

BiomeVegetalDecoration aquaticDecoration::deepLukewarmOcean() { return combine({seagrass(80, 0.8), kelp(80)}); }

BiomeVegetalDecoration aquaticDecoration::river() { return seagrass(48, 0.4); }

BiomeVegetalDecoration aquaticDecoration::swamp() { return seagrass(64, 0.6); }

BiomeVegetalDecoration aquaticDecoration::warmOcean() {
    return combine({coralDecoration::vegetalDecoration(), seagrass(80, 0.3), seaPickles()});
}

const std::set<std::string, std::less<>>& aquaticDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"seagrass_cold",      "seagrass_warm",      "seagrass_river",
                                                           "seagrass_swamp",     "seagrass_normal",    "seagrass_deep",
                                                           "seagrass_deep_cold", "seagrass_deep_warm", "kelp_cold",
                                                           "kelp_warm",          "sea_pickle"};
    return ids;
}

bool aquaticDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                const std::string_view placedFeature) {
    const std::vector<std::int32_t> oceanFloor = chunk.heightmap(HeightmapType::OceanFloor);
    // parity: the shallow variants never place the tall half, the `deep` ones always do.
    if (placedFeature == "seagrass_cold" || placedFeature == "seagrass_warm" || placedFeature == "seagrass_river" ||
        placedFeature == "seagrass_swamp" || placedFeature == "seagrass_normal") {
        placeSeagrass(chunk, random, origin, oceanFloor, 0.0);
        return true;
    }
    if (placedFeature == "seagrass_deep" || placedFeature == "seagrass_deep_cold" ||
        placedFeature == "seagrass_deep_warm") {
        placeSeagrass(chunk, random, origin, oceanFloor, 1.0);
        return true;
    }
    if (placedFeature == "kelp_cold" || placedFeature == "kelp_warm") {
        placeKelp(chunk, random, origin);
        return true;
    }
    if (placedFeature == "sea_pickle") {
        placeSeaPickles(chunk, random, origin, oceanFloor, 20);
        return true;
    }
    return false;
}

} // namespace basalt::server::world::terrain::decoration
