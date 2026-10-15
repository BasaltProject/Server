#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherSurfaceStructuresDecoration.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace basalt::server::world::terrain::decoration::nether {
namespace {

constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kNetherSeaLevel = 32;

using PathSet = std::set<std::string, std::less<>>;

struct Pos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;
};

/// parity: `ColumnFeatureConfiguration`'s two real instances.
struct ColumnConfig {
    std::int32_t minReach;
    std::int32_t maxReach;
    std::int32_t minHeight;
    std::int32_t maxHeight;
};

constexpr ColumnConfig kSmallColumns{.minReach = 1, .maxReach = 1, .minHeight = 1, .maxHeight = 4};
constexpr ColumnConfig kLargeColumns{.minReach = 2, .maxReach = 3, .minHeight = 5, .maxHeight = 10};

const PathSet& deltaCannotReplace() {
    static const PathSet paths = {
        "bedrock", "nether_bricks", "nether_brick_fence", "nether_brick_stairs", "nether_wart", "chest", "spawner"};
    return paths;
}

const PathSet& columnCannotPlaceOn() {
    static const PathSet paths = [] {
        PathSet built = deltaCannotReplace();
        built.insert("lava");
        built.insert("magma_block");
        built.insert("soul_sand");
        return built;
    }();
    return paths;
}

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
    }
    return found->stateId;
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

[[nodiscard]] bool isInChunk(const Chunk& chunk, std::int32_t x, std::int32_t z) {
    return (x >> 4) == chunk.chunkX() && (z >> 4) == chunk.chunkZ();
}

[[nodiscard]] std::optional<std::int32_t> stateAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (!isInChunk(chunk, x, z) || y < bounds.minY() || y > bounds.maxY()) {
        return std::nullopt;
    }
    return chunk.blockStateId(x & 15, y, z & 15);
}

[[nodiscard]] std::string blockPath(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string blockPathAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    return state.has_value() ? blockPath(*state) : std::string{};
}

void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (isInChunk(chunk, x, z) && y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setBlockStateId(x & 15, y, z & 15, stateId);
    }
}

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    return state.has_value() && registry::blocks::isAir(*state);
}

[[nodiscard]] bool isKnownNonAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    return state.has_value() && !registry::blocks::isAir(*state);
}

[[nodiscard]] bool isEmpty(std::int32_t stateId) {
    const std::string path = blockPath(stateId);
    return registry::blocks::isAir(stateId) || path == "water" || path == "lava";
}

[[nodiscard]] std::optional<std::int32_t> findGroundLayer(const Chunk& chunk, std::int32_t x, std::int32_t startY,
                                                          std::int32_t z, std::int32_t targetLayer) {
    std::int32_t currentLayer = 0;
    std::optional<std::int32_t> currentState = stateAt(chunk, x, startY, z);
    for (std::int32_t y = startY; y >= chunk.verticalBounds().minY() + 1; --y) {
        const std::optional<std::int32_t> below = stateAt(chunk, x, y - 1, z);
        const bool surface =
            below.has_value() && !isEmpty(*below) && currentState.has_value() && isEmpty(*currentState);
        if (surface && blockPath(*below) != "bedrock") {
            if (currentLayer == targetLayer) {
                return y;
            }
            currentLayer++;
        }
        currentState = below;
    }
    return std::nullopt;
}

[[nodiscard]] std::vector<Pos> positionsOnEveryLayer(const Chunk& chunk, random::RandomSource& random,
                                                     std::int32_t count) {
    std::vector<Pos> positions;
    std::int32_t layer = 0;
    bool foundAny = false;
    do {
        foundAny = false;
        for (std::int32_t attempt = 0; attempt < count; ++attempt) {
            const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
            const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
            const std::int32_t startY = chunk.heightAt(x & 15, z & 15, HeightmapType::MotionBlocking);
            const std::optional<std::int32_t> y = findGroundLayer(chunk, x, startY, z, layer);
            if (y.has_value()) {
                positions.push_back(Pos{.x = x, .y = *y, .z = z});
                foundAny = true;
            }
        }
        layer++;
    } while (foundAny);
    return positions;
}

[[nodiscard]] bool isDeltaClear(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> current = stateAt(chunk, x, y, z);
    if (!current.has_value()) {
        return false;
    }
    const std::string path = blockPath(*current);
    if (path.empty() || path == "lava" || deltaCannotReplace().contains(path)) {
        return false;
    }
    if (!isAir(chunk, x, y + 1, z)) {
        return false;
    }
    return isKnownNonAir(chunk, x, y - 1, z) && isKnownNonAir(chunk, x, y, z - 1) &&
           isKnownNonAir(chunk, x, y, z + 1) && isKnownNonAir(chunk, x - 1, y, z) && isKnownNonAir(chunk, x + 1, y, z);
}

/// parity: the delta's own diamond walk, which visits by Manhattan depth rather than by rows.
void visitManhattan(std::int32_t reachX, std::int32_t reachZ, std::int32_t radiusLimit,
                    const std::function<void(std::int32_t, std::int32_t)>& visit) {
    for (std::int32_t depth = 0; depth <= radiusLimit; ++depth) {
        const std::int32_t maxX = std::min(reachX, depth);
        for (std::int32_t x = -maxX; x <= maxX; ++x) {
            const std::int32_t z = depth - std::abs(x);
            if (z > reachZ) {
                continue;
            }
            visit(x, z);
            if (z != 0) {
                visit(x, -z);
            }
        }
    }
}

void placeDelta(Chunk& chunk, random::RandomSource& random, const Pos& origin) {
    const bool spawnRim = random.nextDouble() < 0.9;
    const std::int32_t rimX = spawnRim ? random.nextInt(3) : 0;
    const std::int32_t rimZ = spawnRim ? random.nextInt(3) : 0;
    const bool hasRim = spawnRim && rimX != 0 && rimZ != 0;
    const std::int32_t radiusX = 3 + random.nextInt(5);
    const std::int32_t radiusZ = 3 + random.nextInt(5);
    const std::int32_t radiusLimit = std::max(radiusX, radiusZ);
    const std::int32_t magma = stateIdOf("magma_block");
    const std::int32_t lava = stateIdOf("lava");

    visitManhattan(radiusX, radiusZ, radiusLimit, [&](std::int32_t offsetX, std::int32_t offsetZ) {
        const std::int32_t x = origin.x + offsetX;
        const std::int32_t z = origin.z + offsetZ;
        if (!isDeltaClear(chunk, x, origin.y, z)) {
            return;
        }
        if (hasRim) {
            setBlock(chunk, x, origin.y, z, magma);
        }
        const std::int32_t contentX = x + rimX;
        const std::int32_t contentZ = z + rimZ;
        if (isDeltaClear(chunk, contentX, origin.y, contentZ)) {
            setBlock(chunk, contentX, origin.y, contentZ, lava);
        }
    });
}

[[nodiscard]] bool isAirOrLavaOcean(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    if (!state.has_value()) {
        return false;
    }
    return registry::blocks::isAir(*state) || (blockPath(*state) == "lava" && y <= kNetherSeaLevel);
}

[[nodiscard]] bool canPlaceColumnAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    if (!isAirOrLavaOcean(chunk, x, y, z)) {
        return false;
    }
    const std::optional<std::int32_t> belowState = stateAt(chunk, x, y - 1, z);
    if (!belowState.has_value()) {
        return false;
    }
    return !registry::blocks::isAir(*belowState) && !columnCannotPlaceOn().contains(blockPath(*belowState));
}

[[nodiscard]] std::optional<std::int32_t> findSurface(const Chunk& chunk, std::int32_t x, std::int32_t startY,
                                                      std::int32_t z, std::int32_t initialLimit) {
    std::int32_t y = startY;
    std::int32_t limit = initialLimit;
    while (y > chunk.verticalBounds().minY() + 1 && limit > 0) {
        limit--;
        if (canPlaceColumnAt(chunk, x, y, z)) {
            return y;
        }
        y--;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<std::int32_t> findAir(const Chunk& chunk, std::int32_t x, std::int32_t startY,
                                                  std::int32_t z, std::int32_t initialLimit) {
    std::int32_t y = startY;
    std::int32_t limit = initialLimit;
    while (y <= chunk.verticalBounds().maxY() && limit > 0) {
        limit--;
        if (columnCannotPlaceOn().contains(blockPathAt(chunk, x, y, z))) {
            return std::nullopt;
        }
        if (isAir(chunk, x, y, z)) {
            return y;
        }
        y++;
    }
    return std::nullopt;
}

void placeColumn(Chunk& chunk, std::int32_t originX, std::int32_t originY, std::int32_t originZ,
                 std::int32_t columnHeight, std::int32_t reach) {
    const std::int32_t basalt = stateIdOf("basalt");
    for (std::int32_t offsetZ = -reach; offsetZ <= reach; ++offsetZ) {
        for (std::int32_t offsetX = -reach; offsetX <= reach; ++offsetX) {
            const std::int32_t x = originX + offsetX;
            const std::int32_t z = originZ + offsetZ;
            const std::int32_t stepLimit = std::abs(offsetX) + std::abs(offsetZ);
            const std::optional<std::int32_t> startY = isAirOrLavaOcean(chunk, x, originY, z)
                                                           ? findSurface(chunk, x, originY, z, stepLimit)
                                                           : findAir(chunk, x, originY, z, stepLimit);
            if (!startY.has_value()) {
                continue;
            }
            std::int32_t blocksY = columnHeight - stepLimit / 2;
            std::int32_t y = *startY;
            while (blocksY >= 0) {
                if (isAirOrLavaOcean(chunk, x, y, z)) {
                    setBlock(chunk, x, y, z, basalt);
                } else if (blockPathAt(chunk, x, y, z) != "basalt") {
                    break;
                }
                y++;
                blocksY--;
            }
        }
    }
}

void placeColumns(Chunk& chunk, random::RandomSource& random, const Pos& origin, const ColumnConfig& config) {
    if (!canPlaceColumnAt(chunk, origin.x, origin.y, origin.z)) {
        return;
    }
    const std::int32_t columnHeight = config.minHeight + random.nextInt(config.maxHeight - config.minHeight + 1);
    const bool clustered = random.nextFloat() < 0.9F;
    const std::int32_t reach = std::min(columnHeight, clustered ? 5 : 8);
    const std::int32_t count = clustered ? 50 : 15;
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = origin.x - reach + random.nextInt(reach * 2 + 1);
        const std::int32_t y = origin.y + random.nextInt(1);
        const std::int32_t z = origin.z - reach + random.nextInt(reach * 2 + 1);
        const std::int32_t distance = std::abs(x - origin.x) + std::abs(y - origin.y) + std::abs(z - origin.z);
        const std::int32_t blocksToPlace = columnHeight - distance;
        if (blocksToPlace >= 0) {
            const std::int32_t columnReach = config.minReach + random.nextInt(config.maxReach - config.minReach + 1);
            placeColumn(chunk, x, y, z, blocksToPlace, columnReach);
        }
    }
}

void placeFeature(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                  std::int32_t globalIndex) {
    const std::int32_t count = globalIndex == 0 ? 40 : (globalIndex == 1 ? 4 : 2);
    for (const Pos& origin : positionsOnEveryLayer(chunk, random, count)) {
        if (biomePath(biomeSource(origin.x, origin.y, origin.z, nullptr)) != "basalt_deltas") {
            continue;
        }
        if (globalIndex == 0) {
            placeDelta(chunk, random, origin);
        } else if (globalIndex == 1) {
            placeColumns(chunk, random, origin, kSmallColumns);
        } else if (globalIndex == 2) {
            placeColumns(chunk, random, origin, kLargeColumns);
        }
    }
}

} // namespace

const std::set<std::string, std::less<>>& netherSurfaceStructuresDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"delta", "small_basalt_columns", "large_basalt_columns"};
    return ids;
}

void netherSurfaceStructuresDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                                 const NetherBiomeLookup& biomeSource) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    for (std::int32_t globalIndex = 0; globalIndex <= 2; ++globalIndex) {
        random::setFeatureSeed(random, decorationSeed, globalIndex, kSurfaceStructuresStep);
        placeFeature(chunk, random, biomeSource, globalIndex);
    }
}

void netherSurfaceStructuresDecoration::decorateFeature(Chunk& chunk, const std::int64_t seed,
                                                        const NetherBiomeLookup& biomeSource,
                                                        const std::int32_t globalIndex) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, globalIndex, kSurfaceStructuresStep);
    placeFeature(chunk, random, biomeSource, globalIndex);
}

bool netherSurfaceStructuresDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const std::int32_t x,
                                                const std::int32_t y, const std::int32_t z,
                                                const std::string_view placedFeature) {
    const Pos origin{.x = x, .y = y, .z = z};
    if (placedFeature == "delta") {
        placeDelta(chunk, random, origin);
    } else if (placedFeature == "small_basalt_columns") {
        placeColumns(chunk, random, origin, kSmallColumns);
    } else if (placedFeature == "large_basalt_columns") {
        placeColumns(chunk, random, origin, kLargeColumns);
    } else {
        return false;
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration::nether
