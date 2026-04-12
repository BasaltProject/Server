#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/OverworldOreDecoration.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <vector>

namespace basalt::server::world::terrain::decoration {
namespace mth = wire::common::mth;

namespace {

constexpr std::int32_t kUndergroundOresStep = 6;
constexpr std::int32_t kUndergroundDecorationStep = 7;
constexpr std::int32_t kUnderwaterMagmaIndex = 26;
constexpr std::int32_t kDiskGrassIndex = 29;
constexpr std::int32_t kDiskSandIndex = 30;
constexpr std::int32_t kDiskClayIndex = 31;
constexpr std::int32_t kDiskGravelIndex = 32;
constexpr std::int32_t kLastGlobalIndex = 33;
constexpr float kPiFloat = 3.1415927F;

constexpr std::array<std::array<std::int32_t, 3>, 6> kDirections = {
    {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}};

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

[[nodiscard]] bool isWaterBlock(std::optional<std::int32_t> stateId) {
    if (!stateId.has_value()) {
        return false;
    }
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(*stateId);
    return block.has_value() && block->block.path() == "water";
}

void removeContainedSpheres(std::vector<double>& spheres, std::int32_t size) {
    for (std::int32_t first = 0; first < size - 1; ++first) {
        if (spheres[static_cast<std::size_t>(first) * 4 + 3] <= 0.0) {
            continue;
        }
        for (std::int32_t second = first + 1; second < size; ++second) {
            if (spheres[static_cast<std::size_t>(second) * 4 + 3] <= 0.0) {
                continue;
            }
            const double dx =
                spheres[static_cast<std::size_t>(first) * 4] - spheres[static_cast<std::size_t>(second) * 4];
            const double dy =
                spheres[static_cast<std::size_t>(first) * 4 + 1] - spheres[static_cast<std::size_t>(second) * 4 + 1];
            const double dz =
                spheres[static_cast<std::size_t>(first) * 4 + 2] - spheres[static_cast<std::size_t>(second) * 4 + 2];
            const double radiusDelta =
                spheres[static_cast<std::size_t>(first) * 4 + 3] - spheres[static_cast<std::size_t>(second) * 4 + 3];
            if (radiusDelta * radiusDelta > dx * dx + dy * dy + dz * dz) {
                if (radiusDelta > 0.0) {
                    spheres[static_cast<std::size_t>(second) * 4 + 3] = -1.0;
                } else {
                    spheres[static_cast<std::size_t>(first) * 4 + 3] = -1.0;
                }
            }
        }
    }
}

[[nodiscard]] bool isAdjacentToAir(const PlacementRegion& region, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = region.chunk().verticalBounds();
    for (const std::array<std::int32_t, 3>& direction : kDirections) {
        const std::int32_t adjacentY = y + direction[1];
        if (adjacentY < bounds.minY() || adjacentY > bounds.maxY()) {
            return true;
        }
        const std::optional<std::int32_t> stateId = region.blockStateId(x + direction[0], adjacentY, z + direction[2]);
        if (stateId.has_value() && registry::blocks::isAir(*stateId)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool placeTarget(PlacementRegion& region, random::RandomSource& random, std::int32_t x, std::int32_t y,
                               std::int32_t z, const ConfiguredOre& config) {
    const std::optional<std::int32_t> stateId = region.blockStateId(x, y, z);
    if (!stateId.has_value()) {
        return false;
    }
    const std::optional<registry::BlockState> block = registry::blocks::byStateId(*stateId);
    if (!block.has_value()) {
        return false;
    }
    const std::string blockPath = block->block.path();
    const Target* target = nullptr;
    for (const Target& candidate : config.targets) {
        if (targetMatches(candidate, blockPath, y)) {
            target = &candidate;
            break;
        }
    }
    if (target == nullptr) {
        return false;
    }
    bool skipAirCheck = false;
    if (config.discardChanceOnAirExposure <= 0.0F) {
        skipAirCheck = true;
    } else if (config.discardChanceOnAirExposure >= 1.0F) {
        skipAirCheck = false;
    } else {
        skipAirCheck = random.nextFloat() >= config.discardChanceOnAirExposure;
    }
    if (!skipAirCheck && isAdjacentToAir(region, x, y, z)) {
        return false;
    }
    region.setBlockStateId(x, y, z, target->stateId, target->replaceable);
    return true;
}

bool placeVein(PlacementRegion& region, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
               std::int32_t originZ, const ConfiguredOre& config) {
    const float direction = random.nextFloat() * kPiFloat;
    const float spreadXZ = static_cast<float>(config.size) / 8.0F;
    const auto maxRadius =
        static_cast<std::int32_t>(std::ceil((static_cast<double>(config.size) / 16.0 * 2.0 + 1.0) / 2.0));
    const double x0 = originX + std::sin(static_cast<double>(direction)) * static_cast<double>(spreadXZ);
    const double x1 = originX - std::sin(static_cast<double>(direction)) * static_cast<double>(spreadXZ);
    const double z0 = originZ + std::cos(static_cast<double>(direction)) * static_cast<double>(spreadXZ);
    const double z1 = originZ - std::cos(static_cast<double>(direction)) * static_cast<double>(spreadXZ);
    const double y0 = originY + random.nextInt(3) - 2.0;
    const double y1 = originY + random.nextInt(3) - 2.0;
    const auto spreadCeil = static_cast<std::int32_t>(std::ceil(static_cast<double>(spreadXZ)));
    const std::int32_t xStart = originX - spreadCeil - maxRadius;
    const std::int32_t yStart = originY - 2 - maxRadius;
    const std::int32_t zStart = originZ - spreadCeil - maxRadius;
    const std::int32_t sizeXZ = 2 * (spreadCeil + maxRadius);
    const std::int32_t sizeY = 2 * (2 + maxRadius);

    if (!region.hasOceanFloorAtOrAbove(xStart, zStart, sizeXZ, yStart)) {
        return false;
    }

    std::vector<double> spheres(static_cast<std::size_t>(config.size) * 4);
    for (std::int32_t index = 0; index < config.size; ++index) {
        const float step = static_cast<float>(index) / static_cast<float>(config.size);
        const double sphereScale = random.nextDouble() * config.size / 16.0;
        const auto slot = static_cast<std::size_t>(index) * 4;
        spheres[slot] = mth::lerp(static_cast<double>(step), x0, x1);
        spheres[slot + 1] = mth::lerp(static_cast<double>(step), y0, y1);
        spheres[slot + 2] = mth::lerp(static_cast<double>(step), z0, z1);
        spheres[slot + 3] = (static_cast<double>(mth::sin(kPiFloat * step) + 1.0F) * sphereScale + 1.0) / 2.0;
    }
    removeContainedSpheres(spheres, config.size);

    const ChunkHeight bounds = region.chunk().verticalBounds();
    std::int32_t placed = 0;
    std::set<std::int32_t> tested;
    for (std::int32_t index = 0; index < config.size; ++index) {
        const auto slot = static_cast<std::size_t>(index) * 4;
        const double radius = spheres[slot + 3];
        if (radius < 0.0) {
            continue;
        }
        const double centerX = spheres[slot];
        const double centerY = spheres[slot + 1];
        const double centerZ = spheres[slot + 2];
        const std::int32_t minX = std::max(mth::floor(centerX - radius), xStart);
        const std::int32_t minY = std::max(mth::floor(centerY - radius), yStart);
        const std::int32_t minZ = std::max(mth::floor(centerZ - radius), zStart);
        const std::int32_t maxX = std::max(mth::floor(centerX + radius), minX);
        const std::int32_t maxY = std::max(mth::floor(centerY + radius), minY);
        const std::int32_t maxZ = std::max(mth::floor(centerZ + radius), minZ);
        // parity: each distance is divided by the radius and compared against one, which is not the
        // same as comparing squared distances against a squared radius once floating point is
        // involved. A blob this size is mostly edge, and which edge cells are visited also decides
        // how many draws the air-exposure check makes.
        for (std::int32_t x = minX; x <= maxX; ++x) {
            const double xd = (x + 0.5 - centerX) / radius;
            if (xd * xd >= 1.0) {
                continue;
            }
            for (std::int32_t y = minY; y <= maxY; ++y) {
                const double yd = (y + 0.5 - centerY) / radius;
                if (xd * xd + yd * yd >= 1.0) {
                    continue;
                }
                for (std::int32_t z = minZ; z <= maxZ; ++z) {
                    const double zd = (z + 0.5 - centerZ) / radius;
                    if (xd * xd + yd * yd + zd * zd >= 1.0 || y < bounds.minY() || y > bounds.maxY()) {
                        continue;
                    }
                    const std::int32_t testedIndex = x - xStart + (y - yStart) * sizeXZ + (z - zStart) * sizeXZ * sizeY;
                    if (!tested.insert(testedIndex).second) {
                        continue;
                    }
                    if (placeTarget(region, random, x, y, z, config)) {
                        ++placed;
                    }
                }
            }
        }
    }
    return placed > 0;
}

void placePlacedOre(const PlacedOre& ore, PlacementRegion& region, random::RandomSource& random,
                    const surface::SurfaceRuleEnvironment& environment) {
    const Chunk& chunk = region.chunk();
    const std::int32_t attempts = ore.count(random);
    for (std::int32_t attempt = 0; attempt < attempts; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = ore.height(random);
        if (ore.biomes->find(biomePath(environment.biomeAt(x, y, z))) == ore.biomes->end()) {
            continue;
        }
        placeVein(region, random, x, y, z, ore.feature);
    }
}

[[nodiscard]] std::optional<std::int32_t> findWaterColumnFloor(const PlacementRegion& region, std::int32_t x,
                                                               std::int32_t y, std::int32_t z) {
    if (!isWaterBlock(region.blockStateId(x, y, z))) {
        return std::nullopt;
    }
    std::int32_t scanY = y;
    std::int32_t distance = 1;
    while (distance < 5 && isWaterBlock(region.blockStateId(x, scanY, z))) {
        --scanY;
        ++distance;
    }
    if (!isWaterBlock(region.blockStateId(x, scanY, z))) {
        return scanY;
    }
    return std::nullopt;
}

[[nodiscard]] bool hasFullMagmaCover(std::optional<std::int32_t> stateId, registry::BlockFace coveredFace) {
    return stateId.has_value() && registry::blockShapes::isFaceOcclusionFull(*stateId, coveredFace);
}

[[nodiscard]] bool isValidMagmaPlacement(const PlacementRegion& region, std::int32_t x, std::int32_t y,
                                         std::int32_t z) {
    const std::optional<std::int32_t> current = region.blockStateId(x, y, z);
    if (!current.has_value() || registry::blocks::isAir(*current) || isWaterBlock(current)) {
        return false;
    }
    if (!hasFullMagmaCover(region.blockStateId(x, y - 1, z), registry::BlockFace::Up)) {
        return false;
    }
    return hasFullMagmaCover(region.blockStateId(x + 1, y, z), registry::BlockFace::West) &&
           hasFullMagmaCover(region.blockStateId(x - 1, y, z), registry::BlockFace::East) &&
           hasFullMagmaCover(region.blockStateId(x, y, z + 1), registry::BlockFace::North) &&
           hasFullMagmaCover(region.blockStateId(x, y, z - 1), registry::BlockFace::South);
}

void placeUnderwaterMagma(PlacementRegion& region, random::RandomSource& random,
                          const surface::SurfaceRuleEnvironment& environment) {
    const std::int32_t attempts = 44 + random.nextInt(9);
    const std::int32_t minY = region.chunk().verticalBounds().minY();
    for (std::int32_t attempt = 0; attempt < attempts; ++attempt) {
        const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = minY + random.nextInt(321);
        const std::optional<std::int32_t> oceanFloor = region.oceanFloorHeight(x, z);
        if (!oceanFloor.has_value()) {
            continue;
        }
        if (y > *oceanFloor - 2 || allBiomes().find(biomePath(environment.biomeAt(x, y, z))) == allBiomes().end()) {
            continue;
        }
        placeUnderwaterMagmaBody(region, random, x, y, z);
    }
}

[[nodiscard]] std::int32_t grassDiskStateForAbove(std::optional<std::int32_t> aboveStateId) {
    if (aboveStateId.has_value() &&
        (registry::blockShapes::isSolidState(*aboveStateId) || registry::blockFluids::hasWaterFluid(*aboveStateId))) {
        return stateIdOf("dirt");
    }
    return stateIdOf("grass_block");
}

void placeDiskGrass(PlacementRegion& region, random::RandomSource& random,
                    const surface::SurfaceRuleEnvironment& environment) {
    const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
    const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
    const std::optional<std::int32_t> floor = region.oceanFloorHeight(x, z);
    if (!floor.has_value()) {
        return;
    }
    const std::int32_t y = *floor - 1;
    if (region.blockStateId(x, y, z) != stateIdOf("mud")) {
        return;
    }
    if (biomePath(environment.biomeAt(x, y, z)) != "mangrove_swamp") {
        return;
    }
    placeDisk(region, random, x, y, z, grassDisk());
}

void placeWaterDisk(PlacementRegion& region, random::RandomSource& random,
                    const surface::SurfaceRuleEnvironment& environment, const DiskConfig& config, std::int32_t attempts,
                    const BlockSet& biomes) {
    for (std::int32_t attempt = 0; attempt < attempts; ++attempt) {
        const std::int32_t x = region.chunk().chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = region.chunk().chunkZ() * 16 + random.nextInt(16);
        const std::optional<std::int32_t> y = region.oceanFloorHeight(x, z);
        if (!y.has_value()) {
            continue;
        }
        const std::optional<std::int32_t> stateId = region.blockStateId(x, *y, z);
        if (!stateId.has_value() || !registry::blockFluids::hasWaterFluid(*stateId)) {
            continue;
        }
        if (biomes.find(biomePath(environment.biomeAt(x, *y, z))) == biomes.end()) {
            continue;
        }
        placeDisk(region, random, x, *y, z, config);
    }
}

void decorateFeatures(Chunk& chunk, std::int64_t seed, const surface::SurfaceRuleEnvironment& environment,
                      std::int32_t firstIndex, std::int32_t lastIndex, ExternalHeightReader externalSurfaceHeight,
                      ExternalStateReader externalBlockState) {
    PlacementRegion region{chunk, std::move(externalSurfaceHeight), std::move(externalBlockState)};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    for (std::int32_t globalIndex = firstIndex; globalIndex <= lastIndex; ++globalIndex) {
        random::setFeatureSeed(random, decorationSeed, globalIndex, kUndergroundOresStep);
        switch (globalIndex) {
        case kUnderwaterMagmaIndex:
            placeUnderwaterMagma(region, random, environment);
            continue;
        case kDiskGrassIndex:
            placeDiskGrass(region, random, environment);
            continue;
        case kDiskSandIndex:
            placeWaterDisk(region, random, environment, sandDisk(), 3, defaultDiskBiomes());
            continue;
        case kDiskClayIndex:
            placeWaterDisk(region, random, environment, clayDisk(), 1, allBiomes());
            continue;
        case kDiskGravelIndex:
            placeWaterDisk(region, random, environment, gravelDisk(), 1, defaultDiskBiomes());
            continue;
        default:
            break;
        }
        const auto found = oreFeaturesByIndex().find(globalIndex);
        if (found != oreFeaturesByIndex().end()) {
            placePlacedOre(found->second, region, random, environment);
        }
    }
}

} // namespace

const DiskConfig& sandDisk() {
    static const DiskConfig config{2,
                                   6,
                                   2,
                                   {stateIdOf("dirt"), stateIdOf("grass_block")},
                                   [](const PlacementRegion& region, std::int32_t x, std::int32_t y, std::int32_t z) {
                                       return region.blockStateId(x, y - 1, z) == registry::blocks::air().stateId
                                                  ? stateIdOf("sandstone")
                                                  : stateIdOf("sand");
                                   }};
    return config;
}

const DiskConfig& clayDisk() {
    static const DiskConfig config{
        2,
        3,
        1,
        {stateIdOf("dirt"), stateIdOf("clay")},
        [](const PlacementRegion&, std::int32_t, std::int32_t, std::int32_t) { return stateIdOf("clay"); }};
    return config;
}

const DiskConfig& gravelDisk() {
    static const DiskConfig config{
        2,
        5,
        2,
        {stateIdOf("dirt"), stateIdOf("grass_block")},
        [](const PlacementRegion&, std::int32_t, std::int32_t, std::int32_t) { return stateIdOf("gravel"); }};
    return config;
}

const DiskConfig& grassDisk() {
    static const DiskConfig config{2,
                                   6,
                                   2,
                                   {stateIdOf("dirt"), stateIdOf("mud")},
                                   [](const PlacementRegion& region, std::int32_t x, std::int32_t y, std::int32_t z) {
                                       return grassDiskStateForAbove(region.blockStateId(x, y + 1, z));
                                   }};
    return config;
}

void placeDisk(PlacementRegion& region, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
               std::int32_t originZ, const DiskConfig& config) {
    const std::int32_t radius = config.minRadius + random.nextInt(config.maxRadius - config.minRadius + 1);
    const std::int32_t radiusSquared = radius * radius;
    for (std::int32_t z = originZ - radius; z <= originZ + radius; ++z) {
        for (std::int32_t x = originX - radius; x <= originX + radius; ++x) {
            const std::int32_t dx = x - originX;
            const std::int32_t dz = z - originZ;
            if (dx * dx + dz * dz > radiusSquared) {
                continue;
            }
            bool placedAbove = false;
            for (std::int32_t y = originY + config.halfHeight; y >= originY - config.halfHeight; --y) {
                const std::optional<std::int32_t> current = region.blockStateId(x, y, z);
                if (!current.has_value() || config.targets.find(*current) == config.targets.end()) {
                    placedAbove = false;
                    continue;
                }
                region.setBlockStateId(x, y, z, config.state(region, x, y, z));
                // parity: `DiskFeature.placeColumn` marks over the topmost block of each column.
                if (!placedAbove) {
                    region.chunk().markAboveForPostProcessing(x, y, z);
                    placedAbove = true;
                }
            }
        }
    }
}

bool placeUnderwaterMagmaBody(PlacementRegion& region, random::RandomSource& random, std::int32_t x, std::int32_t y,
                              std::int32_t z) {
    const std::optional<std::int32_t> floorY = findWaterColumnFloor(region, x, y, z);
    if (!floorY.has_value()) {
        return false;
    }
    bool placed = false;
    for (std::int32_t placedZ = z - 1; placedZ <= z + 1; ++placedZ) {
        for (std::int32_t placedY = *floorY - 1; placedY <= *floorY + 1; ++placedY) {
            for (std::int32_t placedX = x - 1; placedX <= x + 1; ++placedX) {
                if (random.nextFloat() >= 0.5F || !isValidMagmaPlacement(region, placedX, placedY, placedZ)) {
                    continue;
                }
                region.setBlockStateId(placedX, placedY, placedZ, stateIdOf("magma_block"));
                placed = true;
            }
        }
    }
    return placed;
}

void overworldOreDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                      const surface::SurfaceRuleEnvironment& environment,
                                      ExternalHeightReader externalSurfaceHeight,
                                      ExternalStateReader externalBlockState) {
    decorateFeatures(chunk, seed, environment, 0, kLastGlobalIndex, std::move(externalSurfaceHeight),
                     std::move(externalBlockState));
}

void overworldOreDecoration::decorateInfested(Chunk& chunk, std::int64_t seed,
                                              const surface::SurfaceRuleEnvironment& environment,
                                              ExternalHeightReader externalSurfaceHeight,
                                              ExternalStateReader externalBlockState) {
    PlacementRegion region{chunk, std::move(externalSurfaceHeight), std::move(externalBlockState)};
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kInfestedGlobalIndex, kUndergroundDecorationStep);
    placePlacedOre(infestedOre(), region, random, environment);
}

const std::map<std::string, std::int32_t, std::less<>>& overworldOreDecoration::placedOreIds() {
    static const std::map<std::string, std::int32_t, std::less<>> ids = {
        {"ore_dirt", 0},           {"ore_gravel", 1},
        {"ore_granite_upper", 2},  {"ore_granite_lower", 3},
        {"ore_diorite_upper", 4},  {"ore_diorite_lower", 5},
        {"ore_andesite_upper", 6}, {"ore_andesite_lower", 7},
        {"ore_tuff", 8},           {"ore_coal_upper", 9},
        {"ore_coal_lower", 10},    {"ore_iron_upper", 11},
        {"ore_iron_middle", 12},   {"ore_iron_small", 13},
        {"ore_gold", 14},          {"ore_gold_lower", 15},
        {"ore_redstone", 16},      {"ore_redstone_lower", 17},
        {"ore_diamond", 18},       {"ore_diamond_medium", 19},
        {"ore_diamond_large", 20}, {"ore_diamond_buried", 21},
        {"ore_lapis", 22},         {"ore_lapis_buried", 23},
        {"ore_copper_large", 24},  {"ore_copper", 25},
        {"ore_clay", 27},          {"ore_gold_extra", 28},
        {"ore_emerald", 33},       {"ore_infested", kInfestedGlobalIndex}};
    return ids;
}

const std::set<std::string, std::less<>>& overworldOreDecoration::placedDiskIds() {
    static const std::set<std::string, std::less<>> ids = {"disk_sand", "disk_clay", "disk_gravel", "disk_grass"};
    return ids;
}

bool overworldOreDecoration::placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y,
                                     std::int32_t z, std::string_view placedFeature) {
    const auto named = placedOreIds().find(placedFeature);
    if (named == placedOreIds().end()) {
        return false;
    }
    PlacementRegion region{chunk, nullptr, nullptr};
    if (named->second == kInfestedGlobalIndex) {
        return placeVein(region, random, x, y, z, infestedOre().feature);
    }
    const auto found = oreFeaturesByIndex().find(named->second);
    if (found == oreFeaturesByIndex().end()) {
        return false;
    }
    return placeVein(region, random, x, y, z, found->second.feature);
}

bool overworldOreDecoration::placeDiskAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y,
                                         std::int32_t z, std::string_view placedFeature) {
    const DiskConfig* disk = nullptr;
    if (placedFeature == "disk_sand") {
        disk = &sandDisk();
    } else if (placedFeature == "disk_clay") {
        disk = &clayDisk();
    } else if (placedFeature == "disk_gravel") {
        disk = &gravelDisk();
    } else if (placedFeature == "disk_grass") {
        disk = &grassDisk();
    } else {
        return false;
    }
    PlacementRegion region{chunk, nullptr, nullptr};
    placeDisk(region, random, x, y, z, *disk);
    return true;
}

bool overworldOreDecoration::placeUnderwaterMagmaAt(Chunk& chunk, random::RandomSource& random, std::int32_t x,
                                                    std::int32_t y, std::int32_t z) {
    PlacementRegion region{chunk, nullptr, nullptr};
    return placeUnderwaterMagmaBody(region, random, x, y, z);
}

} // namespace basalt::server::world::terrain::decoration
