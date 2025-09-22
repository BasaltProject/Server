#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/IceSpikesDecoration.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kIceSpikeGlobalIndex = 0;
constexpr std::int32_t kIcePatchGlobalIndex = 1;

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    if (found == nullptr) {
        throw std::runtime_error{"unknown block '" + std::string{path} + "'"};
    }
    return found->stateId;
}

[[nodiscard]] std::set<std::int32_t> stateIdsOf(std::initializer_list<std::string_view> paths) {
    std::set<std::int32_t> ids;
    for (const std::string_view path : paths) {
        ids.insert(stateIdOf(path));
    }
    return ids;
}

const std::set<std::int32_t>& replaceableStateIds() {
    static const std::set<std::int32_t> ids =
        stateIdsOf({"dirt", "coarse_dirt", "podzol", "rooted_dirt", "grass_block", "mycelium", "mud",
                    "muddy_mangrove_roots", "moss_block", "pale_moss_block", "snow_block", "ice"});
    return ids;
}

const std::set<std::int32_t>& icePatchTargetStateIds() {
    static const std::set<std::int32_t> ids =
        stateIdsOf({"dirt", "grass_block", "podzol", "coarse_dirt", "mycelium", "snow_block", "ice"});
    return ids;
}

/// Chunk-local by construction: outside reads as air and outside writes are dropped.
[[nodiscard]] std::int32_t get(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (x >= 0 && x < 16 && z >= 0 && z < 16 && y >= bounds.minY() && y <= bounds.maxY()) {
        return chunk.blockStateId(x, y, z);
    }
    return registry::blocks::air().stateId;
}

void set(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (x >= 0 && x < 16 && z >= 0 && z < 16 && y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setBlockStateId(x, y, z, stateId);
    }
}

void replaceSpikeBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const std::int32_t stateId = get(chunk, x, y, z);
    if (registry::blocks::isAir(stateId) || replaceableStateIds().contains(stateId)) {
        set(chunk, x, y, z, stateIdOf("packed_ice"));
    }
}

void placeSpike(Chunk& chunk, random::RandomSource& random, const DecorationPos& suppliedOrigin) {
    const ChunkHeight bounds = chunk.verticalBounds();
    DecorationPos origin = suppliedOrigin;
    while (registry::blocks::isAir(get(chunk, origin.x, origin.y, origin.z)) && origin.y > bounds.minY() + 2) {
        --origin.y;
    }
    if (get(chunk, origin.x, origin.y, origin.z) != stateIdOf("snow_block")) {
        return;
    }

    origin.y += random.nextInt(4);
    const std::int32_t height = random.nextInt(4) + 7;
    const std::int32_t width = height / 4 + random.nextInt(2);
    // parity: the rare tall spike draws only when the base is already wide, so the draw order
    // depends on the width above it.
    if (width > 1 && random.nextInt(60) == 0) {
        origin.y += 10 + random.nextInt(30);
    }

    for (std::int32_t yOffset = 0; yOffset < height; ++yOffset) {
        const float scale =
            (1.0F - static_cast<float>(yOffset) / static_cast<float>(height)) * static_cast<float>(width);
        const auto newWidth = static_cast<std::int32_t>(std::ceil(scale));
        for (std::int32_t xOffset = -newWidth; xOffset <= newWidth; ++xOffset) {
            const float dx = static_cast<float>(std::abs(xOffset)) - 0.25F;
            for (std::int32_t zOffset = -newWidth; zOffset <= newWidth; ++zOffset) {
                const float dz = static_cast<float>(std::abs(zOffset)) - 0.25F;
                const bool inside = (xOffset == 0 && zOffset == 0) || dx * dx + dz * dz <= scale * scale;
                const bool edge =
                    xOffset == -newWidth || xOffset == newWidth || zOffset == -newWidth || zOffset == newWidth;
                if (inside && (!edge || random.nextFloat() <= 0.75F)) {
                    replaceSpikeBlock(chunk, origin.x + xOffset, origin.y + yOffset, origin.z + zOffset);
                    if (yOffset != 0 && newWidth > 1) {
                        replaceSpikeBlock(chunk, origin.x + xOffset, origin.y - yOffset, origin.z + zOffset);
                    }
                }
            }
        }
    }

    const std::int32_t pillarWidth = std::clamp(width - 1, 0, 1);
    for (std::int32_t xOffset = -pillarWidth; xOffset <= pillarWidth; ++xOffset) {
        for (std::int32_t zOffset = -pillarWidth; zOffset <= pillarWidth; ++zOffset) {
            std::int32_t cursorY = origin.y - 1;
            std::int32_t runLength = (std::abs(xOffset) == 1 && std::abs(zOffset) == 1) ? random.nextInt(5) : 50;
            // parity: the floor is the literal 50, not the world minimum: a spike never roots
            // deeper than that whatever the dimension's bounds are.
            while (cursorY > 50) {
                const std::int32_t stateId = get(chunk, origin.x + xOffset, cursorY, origin.z + zOffset);
                if (!registry::blocks::isAir(stateId) && !replaceableStateIds().contains(stateId) &&
                    stateId != stateIdOf("packed_ice")) {
                    break;
                }
                set(chunk, origin.x + xOffset, cursorY, origin.z + zOffset, stateIdOf("packed_ice"));
                --cursorY;
                --runLength;
                if (runLength <= 0) {
                    cursorY -= random.nextInt(5) + 1;
                    runLength = random.nextInt(5);
                }
            }
        }
    }
}

void placeIcePatch(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin) {
    const std::int32_t radius = 2 + random.nextInt(2);
    const std::int32_t top = origin.y + 1;
    const std::int32_t bottom = origin.y - 2;
    for (std::int32_t x = origin.x - radius; x <= origin.x + radius; ++x) {
        for (std::int32_t z = origin.z - radius; z <= origin.z + radius; ++z) {
            const std::int32_t dx = x - origin.x;
            const std::int32_t dz = z - origin.z;
            if (dx * dx + dz * dz <= radius * radius) {
                for (std::int32_t y = top; y >= bottom + 1; --y) {
                    if (icePatchTargetStateIds().contains(get(chunk, x, y, z))) {
                        set(chunk, x, y, z, stateIdOf("packed_ice"));
                    }
                }
            }
        }
    }
}

[[nodiscard]] bool isIceSpikesBiome(const Chunk& chunk, const surface::SurfaceRuleEnvironment& environment,
                                    std::int32_t localX, std::int32_t y, std::int32_t localZ) {
    const std::string& id = environment.biomeAt(chunk.chunkX() * 16 + localX, y, chunk.chunkZ() * 16 + localZ);
    const std::size_t colon = id.find(':');
    return (colon == std::string::npos ? id : id.substr(colon + 1)) == "ice_spikes";
}

} // namespace

const std::set<std::string, std::less<>>& iceSpikesDecoration::placedIds() {
    static const std::set<std::string, std::less<>> ids = {"ice_spike", "ice_patch"};
    return ids;
}

bool iceSpikesDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const DecorationPos& origin,
                                  std::string_view placedFeature) {
    if (placedFeature == "ice_spike") {
        placeSpike(chunk, random, origin);
        return true;
    }
    if (placedFeature == "ice_patch") {
        placeIcePatch(chunk, random, origin);
        return true;
    }
    return false;
}

void iceSpikesDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                   const surface::SurfaceRuleEnvironment& environment) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kIceSpikeGlobalIndex, kSurfaceStructuresStep);
    for (std::int32_t attempt = 0; attempt < 3; ++attempt) {
        const std::int32_t x = random.nextInt(16);
        const std::int32_t z = random.nextInt(16);
        const std::int32_t y = chunk.heightAt(x, z, HeightmapType::WorldSurface);
        if (!isIceSpikesBiome(chunk, environment, x, y, z)) {
            continue;
        }
        placeSpike(chunk, random, DecorationPos{x, y, z});
    }

    random::setFeatureSeed(random, decorationSeed, kIcePatchGlobalIndex, kSurfaceStructuresStep);
    for (std::int32_t attempt = 0; attempt < 2; ++attempt) {
        const std::int32_t x = random.nextInt(16);
        const std::int32_t z = random.nextInt(16);
        const std::int32_t y = chunk.heightAt(x, z, HeightmapType::WorldSurface) - 1;
        if (get(chunk, x, y, z) == stateIdOf("snow_block") && isIceSpikesBiome(chunk, environment, x, y, z)) {
            placeIcePatch(chunk, random, DecorationPos{x, y, z});
        }
    }
}

} // namespace basalt::server::world::terrain::decoration
