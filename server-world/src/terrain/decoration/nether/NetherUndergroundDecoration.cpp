#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherOreDecoration.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherUndergroundDecoration.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>

namespace basalt::server::world::terrain::decoration::nether {
namespace {

constexpr std::int32_t kUndergroundDecorationStep = 7;
constexpr std::int32_t kPatchAttempts = 96;

using PathSet = std::set<std::string, std::less<>>;

/// parity: the three spring configurations, which differ in what counts as rock and how many holes
/// they demand.
struct Spring {
    const PathSet* validBlocks;
    bool requiresBlockBelow;
    std::int32_t rockCount;
    std::int32_t holeCount;
};

constexpr std::array<std::array<std::int32_t, 3>, 5> kSpringNeighbours{
    std::array<std::int32_t, 3>{-1, 0, 0}, std::array<std::int32_t, 3>{1, 0, 0}, std::array<std::int32_t, 3>{0, 0, -1},
    std::array<std::int32_t, 3>{0, 0, 1}, std::array<std::int32_t, 3>{0, -1, 0}};

constexpr std::array<std::array<std::int32_t, 3>, 6> kAllNeighbours{
    std::array<std::int32_t, 3>{0, -1, 0}, std::array<std::int32_t, 3>{0, 1, 0},  std::array<std::int32_t, 3>{0, 0, -1},
    std::array<std::int32_t, 3>{0, 0, 1},  std::array<std::int32_t, 3>{-1, 0, 0}, std::array<std::int32_t, 3>{1, 0, 0}};

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

const PathSet& allBiomes() {
    static const PathSet paths = {"nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest",
                                  "basalt_deltas"};
    return paths;
}

const PathSet& deltaBiome() {
    static const PathSet paths = {"basalt_deltas"};
    return paths;
}

const PathSet& soulBiome() {
    static const PathSet paths = {"soul_sand_valley"};
    return paths;
}

const PathSet& nonDeltaBiomes() {
    static const PathSet paths = {"nether_wastes", "soul_sand_valley", "crimson_forest", "warped_forest"};
    return paths;
}

const PathSet& soulFireBiomes() {
    static const PathSet paths = {"nether_wastes", "soul_sand_valley", "warped_forest", "basalt_deltas"};
    return paths;
}

const PathSet& netherMushroomBiomes() {
    static const PathSet paths = {"nether_wastes", "basalt_deltas"};
    return paths;
}

const PathSet& glowstoneSupport() {
    static const PathSet paths = {"netherrack", "basalt", "blackstone"};
    return paths;
}

const PathSet& springLavaSupport() {
    static const PathSet paths = {"netherrack", "soul_sand", "gravel", "magma_block", "blackstone"};
    return paths;
}

const PathSet& netherrackOnly() {
    static const PathSet paths = {"netherrack"};
    return paths;
}

const PathSet& crimsonRootSupport() {
    static const PathSet paths = {"dirt",       "coarse_dirt",     "rooted_dirt",   "mud",      "muddy_mangrove_roots",
                                  "moss_block", "pale_moss_block", "grass_block",   "mycelium", "podzol",
                                  "farmland",   "crimson_nylium",  "warped_nylium", "soul_soil"};
    return paths;
}

[[nodiscard]] bool supportsMushroom(const std::string& path) {
    static const PathSet lightOverride = {"mycelium", "podzol", "crimson_nylium", "warped_nylium"};
    static const PathSet solidSupport = {
        "netherrack", "basalt",      "blackstone",      "bedrock",           "soul_sand",     "soul_soil",
        "gravel",     "magma_block", "nether_gold_ore", "nether_quartz_ore", "ancient_debris"};
    return lightOverride.contains(path) || solidSupport.contains(path);
}

const Spring& deltaSpring() {
    static const Spring spring{
        .validBlocks = &springLavaSupport(), .requiresBlockBelow = true, .rockCount = 4, .holeCount = 1};
    return spring;
}

const Spring& openSpring() {
    static const Spring spring{
        .validBlocks = &netherrackOnly(), .requiresBlockBelow = false, .rockCount = 4, .holeCount = 1};
    return spring;
}

const Spring& closedSpring() {
    static const Spring spring{
        .validBlocks = &netherrackOnly(), .requiresBlockBelow = false, .rockCount = 5, .holeCount = 0};
    return spring;
}

[[nodiscard]] std::string blockPath(std::int32_t stateId) {
    const wire::common::Identifier* block = registry::blockStateDefinitions::blockOf(stateId);
    return block == nullptr ? std::string{} : block->path();
}

[[nodiscard]] std::optional<std::int32_t> stateAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return std::nullopt;
    }
    return chunk.decorationBlockStateId(x, y, z);
}

[[nodiscard]] std::optional<std::int32_t> terrainStateAt(const Chunk& chunk, std::int32_t x, std::int32_t y,
                                                         std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    const bool inChunk = (x >> 4) == chunk.chunkX() && (z >> 4) == chunk.chunkZ();
    if (!inChunk || y < bounds.minY() || y > bounds.maxY()) {
        return std::nullopt;
    }
    return chunk.blockStateId(x & 15, y, z & 15);
}

[[nodiscard]] std::string blockPathAt(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    return state.has_value() ? blockPath(*state) : std::string{};
}

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::optional<std::int32_t> state = stateAt(chunk, x, y, z);
    return state.has_value() && registry::blocks::isAir(*state);
}

void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setDecorationBlockStateId(x, y, z, stateId);
    }
}

[[nodiscard]] std::int32_t uniform(random::RandomSource& random, std::int32_t min, std::int32_t max) {
    return min + random.nextInt(max - min + 1);
}

[[nodiscard]] std::int32_t triangle(random::RandomSource& random, std::int32_t range) {
    return random.nextInt(range + 1) - random.nextInt(range + 1);
}

[[nodiscard]] bool hasBiome(const NetherBiomeLookup& biomeSource, std::int32_t x, std::int32_t y, std::int32_t z,
                            const PathSet& biomes) {
    return biomes.contains(biomePath(biomeSource(x, y, z, nullptr)));
}

void placePatchAt(Chunk& chunk, random::RandomSource& random, std::int32_t baseX, std::int32_t baseY,
                  std::int32_t baseZ, std::string_view support, std::string_view fire) {
    for (std::int32_t attempt = 0; attempt < kPatchAttempts; ++attempt) {
        const std::int32_t x = baseX + triangle(random, 7);
        const std::int32_t y = baseY + triangle(random, 3);
        const std::int32_t z = baseZ + triangle(random, 7);
        if (isAir(chunk, x, y, z) && blockPathAt(chunk, x, y - 1, z) == support) {
            setBlock(chunk, x, y, z, stateIdOf(fire));
        }
    }
}

void placeRootsAt(Chunk& chunk, random::RandomSource& random, std::int32_t baseX, std::int32_t baseY,
                  std::int32_t baseZ) {
    for (std::int32_t attempt = 0; attempt < kPatchAttempts; ++attempt) {
        const std::int32_t x = baseX + triangle(random, 7);
        const std::int32_t y = baseY + triangle(random, 3);
        const std::int32_t z = baseZ + triangle(random, 7);
        if (isAir(chunk, x, y, z) && crimsonRootSupport().contains(blockPathAt(chunk, x, y - 1, z))) {
            setBlock(chunk, x, y, z, stateIdOf("crimson_roots"));
        }
    }
}

void placeMushroomAt(Chunk& chunk, random::RandomSource& random, std::int32_t baseX, std::int32_t baseY,
                     std::int32_t baseZ, std::string_view mushroom) {
    for (std::int32_t attempt = 0; attempt < kPatchAttempts; ++attempt) {
        const std::int32_t x = baseX + triangle(random, 7);
        const std::int32_t y = baseY + triangle(random, 3);
        const std::int32_t z = baseZ + triangle(random, 7);
        const std::string support = blockPathAt(chunk, x, y - 1, z);
        if (isAir(chunk, x, y, z) && !support.empty() && supportsMushroom(support)) {
            setBlock(chunk, x, y, z, stateIdOf(mushroom));
        }
    }
}

void replaceBlob(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t startY, std::int32_t z,
                 std::string_view replacement) {
    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t centerY = startY;
    while (centerY > bounds.minY() + 1 && blockPathAt(chunk, x, centerY, z) != "netherrack") {
        centerY--;
    }
    if (centerY <= bounds.minY() + 1) {
        return;
    }
    const std::int32_t radiusX = 3 + random.nextInt(5);
    const std::int32_t radiusY = 3 + random.nextInt(5);
    const std::int32_t radiusZ = 3 + random.nextInt(5);
    const std::int32_t maximumRadius = std::max({radiusX, radiusY, radiusZ});
    const std::int32_t replacementState = stateIdOf(replacement);
    for (std::int32_t offsetX = -radiusX; offsetX <= radiusX; ++offsetX) {
        for (std::int32_t offsetY = -radiusY; offsetY <= radiusY; ++offsetY) {
            for (std::int32_t offsetZ = -radiusZ; offsetZ <= radiusZ; ++offsetZ) {
                if (std::abs(offsetX) + std::abs(offsetY) + std::abs(offsetZ) > maximumRadius) {
                    continue;
                }
                const std::int32_t worldX = x + offsetX;
                const std::int32_t worldY = centerY + offsetY;
                const std::int32_t worldZ = z + offsetZ;
                if (blockPathAt(chunk, worldX, worldY, worldZ) == "netherrack") {
                    setBlock(chunk, worldX, worldY, worldZ, replacementState);
                }
            }
        }
    }
}

void placeSpring(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, const Spring& spring) {
    const auto valid = [&spring](const std::string& path) { return spring.validBlocks->contains(path); };
    const std::optional<std::int32_t> above = terrainStateAt(chunk, x, y + 1, z);
    if (!above.has_value() || !valid(blockPath(*above))) {
        return;
    }
    if (spring.requiresBlockBelow) {
        const std::optional<std::int32_t> below = terrainStateAt(chunk, x, y - 1, z);
        if (!below.has_value() || !valid(blockPath(*below))) {
            return;
        }
    }
    const std::optional<std::int32_t> current = terrainStateAt(chunk, x, y, z);
    if (!current.has_value()) {
        return;
    }
    if (!registry::blocks::isAir(*current) && !valid(blockPath(*current))) {
        return;
    }
    std::int32_t rockCount = 0;
    std::int32_t holeCount = 0;
    for (const std::array<std::int32_t, 3>& offset : kSpringNeighbours) {
        const std::optional<std::int32_t> neighbour =
            terrainStateAt(chunk, x + offset[0], y + offset[1], z + offset[2]);
        if (neighbour.has_value() && valid(blockPath(*neighbour))) {
            ++rockCount;
        }
        if (neighbour.has_value() && registry::blocks::isAir(*neighbour)) {
            ++holeCount;
        }
    }
    if (rockCount == spring.rockCount && holeCount == spring.holeCount) {
        setBlock(chunk, x, y, z, stateIdOf("lava"));
    }
}

void placeGlowstoneBlob(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                        std::int32_t originZ) {
    if (!isAir(chunk, originX, originY, originZ)) {
        return;
    }
    if (!glowstoneSupport().contains(blockPathAt(chunk, originX, originY + 1, originZ))) {
        return;
    }
    const std::int32_t glowstone = stateIdOf("glowstone");
    setBlock(chunk, originX, originY, originZ, glowstone);
    for (std::int32_t attempt = 0; attempt < 1500; ++attempt) {
        const std::int32_t x = originX + random.nextInt(8) - random.nextInt(8);
        const std::int32_t y = originY - random.nextInt(12);
        const std::int32_t z = originZ + random.nextInt(8) - random.nextInt(8);
        if (!isAir(chunk, x, y, z)) {
            continue;
        }
        std::int32_t neighbours = 0;
        for (const std::array<std::int32_t, 3>& offset : kAllNeighbours) {
            if (blockPathAt(chunk, x + offset[0], y + offset[1], z + offset[2]) == "glowstone") {
                ++neighbours;
            }
            if (neighbours > 1) {
                break;
            }
        }
        if (neighbours == 1) {
            setBlock(chunk, x, y, z, glowstone);
        }
    }
}

void placeReplaceBlobs(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                       std::int32_t count, std::string_view replacement) {
    const ChunkHeight bounds = chunk.verticalBounds();
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = random.nextInt(128);
        if (!hasBiome(biomeSource, x, y, z, deltaBiome())) {
            continue;
        }
        replaceBlob(chunk, random, x, std::clamp(y, 1, bounds.maxY()), z, replacement);
    }
}

void placeSprings(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource, std::int32_t count,
                  std::int32_t minY, std::int32_t maxY, const PathSet& biomes, const Spring& spring) {
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = uniform(random, minY, maxY);
        if (hasBiome(biomeSource, x, y, z, biomes)) {
            placeSpring(chunk, x, y, z, spring);
        }
    }
}

void placeFirePatch(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                    const PathSet& biomes, std::string_view support, std::string_view fire) {
    const std::int32_t outer = random.nextInt(6);
    for (std::int32_t attempt = 0; attempt < outer; ++attempt) {
        const std::int32_t baseX = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t baseZ = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t baseY = uniform(random, 4, 123);
        if (!hasBiome(biomeSource, baseX, baseY, baseZ, biomes)) {
            continue;
        }
        placePatchAt(chunk, random, baseX, baseY, baseZ, support, fire);
    }
}

void placeGlowstone(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                    std::int32_t count, std::int32_t minY, std::int32_t maxY) {
    for (std::int32_t attempt = 0; attempt < count; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = uniform(random, minY, maxY);
        if (hasBiome(biomeSource, x, y, z, allBiomes())) {
            placeGlowstoneBlob(chunk, random, x, y, z);
        }
    }
}

void placeCrimsonRootsPatch(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource) {
    const std::int32_t baseX = chunk.chunkX() * 16;
    const std::int32_t baseZ = chunk.chunkZ() * 16;
    const std::int32_t baseY = random.nextInt(128);
    if (!hasBiome(biomeSource, baseX, baseY, baseZ, soulBiome())) {
        return;
    }
    placeRootsAt(chunk, random, baseX, baseY, baseZ);
}

void placeMushroomPatch(Chunk& chunk, random::RandomSource& random, const NetherBiomeLookup& biomeSource,
                        std::string_view mushroom) {
    if (random.nextFloat() >= 0.5F) {
        return;
    }
    const std::int32_t baseX = chunk.chunkX() * 16 + random.nextInt(16);
    const std::int32_t baseZ = chunk.chunkZ() * 16 + random.nextInt(16);
    const std::int32_t baseY = random.nextInt(128);
    if (!hasBiome(biomeSource, baseX, baseY, baseZ, netherMushroomBiomes())) {
        return;
    }
    placeMushroomAt(chunk, random, baseX, baseY, baseZ, mushroom);
}

void placeFeature(std::int32_t globalIndex, Chunk& chunk, random::RandomSource& random,
                  const NetherBiomeLookup& biomeSource) {
    switch (globalIndex) {
    case 0:
        placeReplaceBlobs(chunk, random, biomeSource, 75, "basalt");
        return;
    case 1:
        placeReplaceBlobs(chunk, random, biomeSource, 25, "blackstone");
        return;
    case 2:
        placeSprings(chunk, random, biomeSource, 16, 4, 123, deltaBiome(), deltaSpring());
        return;
    case 3:
        placeSprings(chunk, random, biomeSource, 8, 4, 123, nonDeltaBiomes(), openSpring());
        return;
    case 4:
        placeFirePatch(chunk, random, biomeSource, allBiomes(), "netherrack", "fire");
        return;
    case 5:
        placeFirePatch(chunk, random, biomeSource, soulFireBiomes(), "soul_soil", "soul_fire");
        return;
    case 6: {
        const std::int32_t count = random.nextInt(random.nextInt(10) + 1);
        placeGlowstone(chunk, random, biomeSource, count, 4, 123);
        return;
    }
    case 7:
        placeGlowstone(chunk, random, biomeSource, 10, 0, 127);
        return;
    case 8:
        placeCrimsonRootsPatch(chunk, random, biomeSource);
        return;
    case 9:
        placeMushroomPatch(chunk, random, biomeSource, "brown_mushroom");
        return;
    case 10:
        placeMushroomPatch(chunk, random, biomeSource, "red_mushroom");
        return;
    case 12:
        placeSprings(chunk, random, biomeSource, 32, 10, 117, deltaBiome(), closedSpring());
        return;
    case 15:
        placeSprings(chunk, random, biomeSource, 16, 10, 117, nonDeltaBiomes(), closedSpring());
        return;
    default:
        return;
    }
}

} // namespace

const std::set<std::string, std::less<>>& netherUndergroundDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {
        "basalt_blobs",        "blackstone_blobs",      "spring_delta",       "spring_open",
        "patch_fire",          "patch_soul_fire",       "glowstone_extra",    "glowstone",
        "patch_crimson_roots", "brown_mushroom_nether", "red_mushroom_nether"};
    return ids;
}

void netherUndergroundDecoration::decorate(Chunk& chunk, const std::int64_t seed,
                                           const NetherBiomeLookup& biomeSource) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    for (std::int32_t globalIndex = 0; globalIndex <= 22; ++globalIndex) {
        random::setFeatureSeed(random, decorationSeed, globalIndex, kUndergroundDecorationStep);
        if (netherOreDecoration::placeFeature(globalIndex, chunk, random, biomeSource)) {
            continue;
        }
        placeFeature(globalIndex, chunk, random, biomeSource);
    }
}

void netherUndergroundDecoration::decorateFeature(Chunk& chunk, const std::int64_t seed,
                                                  const NetherBiomeLookup& biomeSource,
                                                  const std::int32_t globalIndex) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, globalIndex, kUndergroundDecorationStep);
    if (!netherOreDecoration::placeFeature(globalIndex, chunk, random, biomeSource)) {
        placeFeature(globalIndex, chunk, random, biomeSource);
    }
}

bool netherUndergroundDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const std::int32_t x,
                                          const std::int32_t y, const std::int32_t z,
                                          const std::string_view placedFeature) {
    if (placedFeature == "basalt_blobs") {
        replaceBlob(chunk, random, x, y, z, "basalt");
    } else if (placedFeature == "blackstone_blobs") {
        replaceBlob(chunk, random, x, y, z, "blackstone");
    } else if (placedFeature == "spring_delta") {
        placeSpring(chunk, x, y, z, deltaSpring());
    } else if (placedFeature == "spring_open") {
        placeSpring(chunk, x, y, z, openSpring());
    } else if (placedFeature == "patch_fire") {
        placePatchAt(chunk, random, x, y, z, "netherrack", "fire");
    } else if (placedFeature == "patch_soul_fire") {
        placePatchAt(chunk, random, x, y, z, "soul_soil", "soul_fire");
    } else if (placedFeature == "glowstone_extra" || placedFeature == "glowstone") {
        placeGlowstoneBlob(chunk, random, x, y, z);
    } else if (placedFeature == "patch_crimson_roots") {
        placeRootsAt(chunk, random, x, y, z);
    } else if (placedFeature == "brown_mushroom_nether") {
        placeMushroomAt(chunk, random, x, y, z, "brown_mushroom");
    } else if (placedFeature == "red_mushroom_nether") {
        placeMushroomAt(chunk, random, x, y, z, "red_mushroom");
    } else {
        return false;
    }
    return true;
}

} // namespace basalt::server::world::terrain::decoration::nether
