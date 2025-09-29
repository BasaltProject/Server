#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherOreDecoration.hpp>
#include <basalt/wire/common/Mth.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::nether {
namespace {

constexpr std::int32_t kUndergroundDecorationStep = 7;
constexpr float kPi = 3.1415927F;

using PathSet = std::set<std::string, std::less<>>;
using Height = std::function<std::int32_t(random::RandomSource&)>;

enum class Target : std::uint8_t { Netherrack, BaseStoneNether };

struct ConfiguredOre {
    std::int32_t size = 0;
    std::int32_t stateId = 0;
    Target target = Target::Netherrack;
    /// A scattered ore walks outward from its origin; a vein sweeps overlapping spheres along a line.
    bool scattered = false;
};

struct PlacedOre {
    std::int32_t globalIndex = 0;
    std::int32_t count = 0;
    Height height;
    const PathSet* biomes = nullptr;
    ConfiguredOre feature;
};

constexpr std::array<std::array<std::int32_t, 3>, 6> kDirections{
    std::array<std::int32_t, 3>{1, 0, 0}, std::array<std::int32_t, 3>{-1, 0, 0},
    std::array<std::int32_t, 3>{0, 1, 0}, std::array<std::int32_t, 3>{0, -1, 0},
    std::array<std::int32_t, 3>{0, 0, 1}, std::array<std::int32_t, 3>{0, 0, -1}};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"missing vanilla block minecraft:" + std::string{path}};
    }
    return found->stateId;
}

[[nodiscard]] std::string pathOf(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::string_view biomePath(std::string_view id) {
    const std::size_t colon = id.find(':');
    return colon == std::string_view::npos ? id : id.substr(colon + 1);
}

const PathSet& allBiomes() {
    static const PathSet paths = {"nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest",
                                  "basalt_deltas"};
    return paths;
}

const PathSet& nonDeltaBiomes() {
    static const PathSet paths = {"nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest"};
    return paths;
}

const PathSet& deltaBiomes() {
    static const PathSet paths = {"basalt_deltas"};
    return paths;
}

const PathSet& soulSandValley() {
    static const PathSet paths = {"soul_sand_valley"};
    return paths;
}

const PathSet& baseStoneNether() {
    static const PathSet paths = {"netherrack", "basalt", "blackstone"};
    return paths;
}

[[nodiscard]] bool isInChunk(const Chunk& chunk, std::int32_t worldX, std::int32_t worldZ) {
    return (worldX >> 4) == chunk.chunkX() && (worldZ >> 4) == chunk.chunkZ();
}

[[nodiscard]] bool isAdjacentToAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    for (const std::array<std::int32_t, 3>& direction : kDirections) {
        const std::int32_t adjacentX = x + direction[0];
        const std::int32_t adjacentY = y + direction[1];
        const std::int32_t adjacentZ = z + direction[2];
        if (!isInChunk(chunk, adjacentX, adjacentZ) || adjacentY < bounds.minY() || adjacentY > bounds.maxY()) {
            continue;
        }
        if (registry::blocks::isAir(chunk.blockStateId(adjacentX & 15, adjacentY, adjacentZ & 15))) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool canPlace(const Chunk& chunk, std::int32_t worldX, std::int32_t y, std::int32_t worldZ, Target target,
                            bool discardOnAirExposure) {
    std::int32_t stateId = 0;
    if (isInChunk(chunk, worldX, worldZ)) {
        stateId = chunk.blockStateId(worldX & 15, y, worldZ & 15);
    } else {
        if (target != Target::Netherrack || discardOnAirExposure) {
            return false;
        }
        stateId = stateIdOf("netherrack");
    }
    const std::string path = pathOf(stateId);
    if (path.empty()) {
        return false;
    }
    const bool targetMatches = target == Target::Netherrack ? path == "netherrack" : baseStoneNether().contains(path);
    return targetMatches && (!discardOnAirExposure || !isAdjacentToAir(chunk, worldX, y, worldZ));
}

/// parity: `OreFeature`'s own pass that drops a sphere wholly inside another.
void removeContainedSpheres(std::vector<double>& spheres, std::int32_t size) {
    for (std::int32_t first = 0; first < size - 1; ++first) {
        if (spheres[static_cast<std::size_t>(first * 4 + 3)] <= 0.0) {
            continue;
        }
        for (std::int32_t second = first + 1; second < size; ++second) {
            if (spheres[static_cast<std::size_t>(second * 4 + 3)] <= 0.0) {
                continue;
            }
            const double dx =
                spheres[static_cast<std::size_t>(first * 4)] - spheres[static_cast<std::size_t>(second * 4)];
            const double dy =
                spheres[static_cast<std::size_t>(first * 4 + 1)] - spheres[static_cast<std::size_t>(second * 4 + 1)];
            const double dz =
                spheres[static_cast<std::size_t>(first * 4 + 2)] - spheres[static_cast<std::size_t>(second * 4 + 2)];
            const double radiusDelta =
                spheres[static_cast<std::size_t>(first * 4 + 3)] - spheres[static_cast<std::size_t>(second * 4 + 3)];
            if (radiusDelta * radiusDelta > dx * dx + dy * dy + dz * dz) {
                if (radiusDelta > 0.0) {
                    spheres[static_cast<std::size_t>(second * 4 + 3)] = -1.0;
                } else {
                    spheres[static_cast<std::size_t>(first * 4 + 3)] = -1.0;
                }
            }
        }
    }
}

bool placeVein(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
               std::int32_t originZ, const ConfiguredOre& config) {
    const float direction = random.nextFloat() * kPi;
    const float spreadXZ = static_cast<float>(config.size) / 8.0F;
    const auto maxRadius = static_cast<std::int32_t>(
        std::ceil(static_cast<double>((static_cast<float>(config.size) / 16.0F * 2.0F + 1.0F) / 2.0F)));
    const double spread = static_cast<double>(spreadXZ);
    const double x0 = originX + std::sin(static_cast<double>(direction)) * spread;
    const double x1 = originX - std::sin(static_cast<double>(direction)) * spread;
    const double z0 = originZ + std::cos(static_cast<double>(direction)) * spread;
    const double z1 = originZ - std::cos(static_cast<double>(direction)) * spread;
    const double y0 = originY + random.nextInt(3) - 2.0;
    const double y1 = originY + random.nextInt(3) - 2.0;
    const auto spreadCeil = static_cast<std::int32_t>(std::ceil(spread));
    const std::int32_t xStart = originX - spreadCeil - maxRadius;
    const std::int32_t yStart = originY - 2 - maxRadius;
    const std::int32_t zStart = originZ - spreadCeil - maxRadius;
    const std::int32_t sizeXZ = 2 * (spreadCeil + maxRadius);
    const std::int32_t sizeY = 2 * (2 + maxRadius);

    // The Nether's protected solid roof makes OreFeature's ocean-floor precheck true for every valid
    // configured height. It consumes no random, so it folds away here.
    std::vector<double> spheres(static_cast<std::size_t>(config.size) * 4, 0.0);
    for (std::int32_t index = 0; index < config.size; ++index) {
        const float step = static_cast<float>(index) / static_cast<float>(config.size);
        const double sphereScale = random.nextDouble() * config.size / 16.0;
        const auto at = static_cast<std::size_t>(index * 4);
        spheres[at] = wire::common::mth::lerp(static_cast<double>(step), x0, x1);
        spheres[at + 1] = wire::common::mth::lerp(static_cast<double>(step), y0, y1);
        spheres[at + 2] = wire::common::mth::lerp(static_cast<double>(step), z0, z1);
        spheres[at + 3] = (static_cast<double>(wire::common::mth::sin(kPi * step) + 1.0F) * sphereScale + 1.0) / 2.0;
    }
    removeContainedSpheres(spheres, config.size);

    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t placed = 0;
    std::set<std::int32_t> tested;
    for (std::int32_t index = 0; index < config.size; ++index) {
        const auto at = static_cast<std::size_t>(index * 4);
        const double radius = spheres[at + 3];
        if (radius < 0.0) {
            continue;
        }
        const double centerX = spheres[at];
        const double centerY = spheres[at + 1];
        const double centerZ = spheres[at + 2];
        const std::int32_t minX = std::max(wire::common::mth::floor(centerX - radius), xStart);
        const std::int32_t minY = std::max(wire::common::mth::floor(centerY - radius), yStart);
        const std::int32_t minZ = std::max(wire::common::mth::floor(centerZ - radius), zStart);
        const std::int32_t maxX = std::max(wire::common::mth::floor(centerX + radius), minX);
        const std::int32_t maxY = std::max(wire::common::mth::floor(centerY + radius), minY);
        const std::int32_t maxZ = std::max(wire::common::mth::floor(centerZ + radius), minZ);
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
                    if (!tested.insert(testedIndex).second || y <= bounds.minY()) {
                        continue;
                    }
                    if (canPlace(chunk, x, y, z, config.target, false)) {
                        chunk.setDecorationBlockStateId(x, y, z, config.stateId, stateIdOf("netherrack"));
                        ++placed;
                    }
                }
            }
        }
    }
    return placed > 0;
}

[[nodiscard]] std::int32_t randomOffset(random::RandomSource& random, std::int32_t distance) {
    const float value = (random.nextFloat() - random.nextFloat()) * static_cast<float>(distance);
    return static_cast<std::int32_t>(std::floor(static_cast<double>(value) + 0.5));
}

void placeScattered(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                    std::int32_t originZ, const ConfiguredOre& config) {
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t tries = random.nextInt(config.size + 1);
    for (std::int32_t index = 0; index < tries; ++index) {
        const std::int32_t distance = std::min(index, 7);
        const std::int32_t x = originX + randomOffset(random, distance);
        const std::int32_t y = originY + randomOffset(random, distance);
        const std::int32_t z = originZ + randomOffset(random, distance);
        if (!isInChunk(chunk, x, z) || y < bounds.minY() || y > bounds.maxY()) {
            continue;
        }
        if (canPlace(chunk, x, y, z, config.target, true)) {
            chunk.setBlockStateId(x & 15, y, z & 15, config.stateId);
        }
    }
}

[[nodiscard]] Height uniform(std::int32_t min, std::int32_t max) {
    return [min, max](random::RandomSource& random) { return min + random.nextInt(max - min + 1); };
}

[[nodiscard]] Height triangle8To24() {
    return [](random::RandomSource& random) { return 8 + random.nextInt(9) + random.nextInt(9); };
}

// parity: FeatureSorter's global indexes for the five Nether biome feature lists in 26.2.
const std::vector<PlacedOre>& features() {
    static const std::vector<PlacedOre> built = [] {
        const auto vein = [](std::int32_t index, std::int32_t count, Height height, const PathSet& biomes,
                             std::int32_t size, std::string_view block) {
            return PlacedOre{
                .globalIndex = index,
                .count = count,
                .height = std::move(height),
                .biomes = &biomes,
                .feature = ConfiguredOre{
                    .size = size, .stateId = stateIdOf(block), .target = Target::Netherrack, .scattered = false}};
        };
        const auto scattered = [](std::int32_t index, Height height, std::int32_t size) {
            return PlacedOre{.globalIndex = index,
                             .count = 1,
                             .height = std::move(height),
                             .biomes = &allBiomes(),
                             .feature = ConfiguredOre{.size = size,
                                                      .stateId = stateIdOf("ancient_debris"),
                                                      .target = Target::BaseStoneNether,
                                                      .scattered = true}};
        };
        std::vector<PlacedOre> ores;
        ores.push_back(vein(11, 4, uniform(27, 36), allBiomes(), 33, "magma_block"));
        ores.push_back(vein(13, 20, uniform(10, 117), deltaBiomes(), 10, "nether_gold_ore"));
        ores.push_back(vein(14, 32, uniform(10, 117), deltaBiomes(), 14, "nether_quartz_ore"));
        ores.push_back(vein(16, 12, uniform(0, 31), soulSandValley(), 12, "soul_sand"));
        ores.push_back(vein(17, 2, uniform(5, 41), nonDeltaBiomes(), 33, "gravel"));
        ores.push_back(vein(18, 2, uniform(5, 31), nonDeltaBiomes(), 33, "blackstone"));
        ores.push_back(vein(19, 10, uniform(10, 117), nonDeltaBiomes(), 10, "nether_gold_ore"));
        ores.push_back(vein(20, 16, uniform(10, 117), nonDeltaBiomes(), 14, "nether_quartz_ore"));
        ores.push_back(scattered(21, triangle8To24(), 3));
        ores.push_back(scattered(22, uniform(8, 119), 2));
        return ores;
    }();
    return built;
}

[[nodiscard]] const PlacedOre* byIndex(std::int32_t globalIndex) {
    for (const PlacedOre& ore : features()) {
        if (ore.globalIndex == globalIndex) {
            return &ore;
        }
    }
    return nullptr;
}

void place(const PlacedOre& ore, Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource) {
    for (std::int32_t attempt = 0; attempt < ore.count; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = ore.height(random);
        if (!ore.biomes->contains(biomePath(biomeSource(x, y, z, nullptr)))) {
            continue;
        }
        if (ore.feature.scattered) {
            placeScattered(chunk, random, x, y, z, ore.feature);
        } else {
            static_cast<void>(placeVein(chunk, random, x, y, z, ore.feature));
        }
    }
}

} // namespace

const std::vector<std::pair<std::string, std::int32_t>>& netherOreDecoration::placedIds() {
    static const std::vector<std::pair<std::string, std::int32_t>> ids = {
        {"ore_magma", 11},       {"ore_gold_deltas", 13},   {"ore_quartz_deltas", 14},
        {"ore_soul_sand", 16},   {"ore_gravel_nether", 17}, {"ore_blackstone", 18},
        {"ore_gold_nether", 19}, {"ore_quartz_nether", 20}, {"ore_ancient_debris_large", 21},
        {"ore_debris_small", 22}};
    return ids;
}

void netherOreDecoration::decorate(Chunk& chunk, const std::int64_t seed, const NetherBiomeLookup& biomeSource) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    for (const PlacedOre& ore : features()) {
        random::setFeatureSeed(random, decorationSeed, ore.globalIndex, kUndergroundDecorationStep);
        place(ore, chunk, random, biomeSource);
    }
}

bool netherOreDecoration::placeFeature(const std::int32_t globalIndex, Chunk& chunk, random::RandomSource& random,
                                       const NetherBiomeLookup& biomeSource) {
    const PlacedOre* ore = byIndex(globalIndex);
    if (ore == nullptr) {
        return false;
    }
    place(*ore, chunk, random, biomeSource);
    return true;
}

bool netherOreDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const std::int32_t x,
                                  const std::int32_t y, const std::int32_t z, const std::string_view placedFeature) {
    std::int32_t globalIndex = -1;
    for (const auto& [name, index] : placedIds()) {
        if (name == placedFeature) {
            globalIndex = index;
            break;
        }
    }
    const PlacedOre* ore = globalIndex < 0 ? nullptr : byIndex(globalIndex);
    if (ore == nullptr) {
        return false;
    }
    if (ore->feature.scattered) {
        placeScattered(chunk, random, x, y, z, ore->feature);
    } else {
        static_cast<void>(placeVein(chunk, random, x, y, z, ore->feature));
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration::nether
