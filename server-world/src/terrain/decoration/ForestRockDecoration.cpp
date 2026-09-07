#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateDefinitions.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/ForestRockDecoration.hpp>

#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration {
namespace {

constexpr std::int32_t kLocalModificationsStep = 2;
constexpr std::int32_t kGlobalIndex = 4;

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

const std::set<std::string, std::less<>>& oldGrowthTaigaBiomes() {
    static const std::set<std::string, std::less<>> paths = {"old_growth_pine_taiga", "old_growth_spruce_taiga"};
    return paths;
}

const std::set<std::string, std::less<>>& supportBlocks() {
    static const std::set<std::string, std::less<>> paths = {
        "dirt",       "coarse_dirt",     "rooted_dirt", "mud",      "muddy_mangrove_roots",
        "moss_block", "pale_moss_block", "grass_block", "podzol",   "mycelium",
        "stone",      "granite",         "diorite",     "andesite", "tuff",
        "deepslate"};
    return paths;
}

/// parity: a boulder is up to three blobs whose origin drifts by one per blob, so its body reaches
/// past the chunk edge often. Vanilla writes there through the `WorldGenLevel`; dropping those writes
/// loses the half of every boulder that a neighbouring chunk seeded. Coordinates are world ones.
[[nodiscard]] std::int32_t get(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return registry::blocks::air().stateId;
    }
    return chunk.decorationBlockStateId(x, y, z);
}

void set(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setDecorationBlockStateId(x, y, z, stateId);
    }
}

[[nodiscard]] bool canPlaceOn(std::int32_t stateId) {
    return supportBlocks().find(pathOf(stateId)) != supportBlocks().end();
}

} // namespace

void forestRockDecoration::placeAt(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y,
                                   std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t originX = x;
    std::int32_t originY = y;
    std::int32_t originZ = z;
    while (originY > bounds.minY() + 3 && !canPlaceOn(get(chunk, originX, originY - 1, originZ))) {
        --originY;
    }
    if (originY <= bounds.minY() + 3) {
        return;
    }

    for (std::int32_t blob = 0; blob < 3; ++blob) {
        const std::int32_t radiusX = random.nextInt(2);
        const std::int32_t radiusY = random.nextInt(2);
        const std::int32_t radiusZ = random.nextInt(2);
        const float radius = static_cast<float>(radiusX + radiusY + radiusZ) * 0.333F + 0.5F;
        const float radiusSquared = radius * radius;
        for (std::int32_t blobZ = originZ - radiusZ; blobZ <= originZ + radiusZ; ++blobZ) {
            for (std::int32_t blobY = originY - radiusY; blobY <= originY + radiusY; ++blobY) {
                for (std::int32_t blobX = originX - radiusX; blobX <= originX + radiusX; ++blobX) {
                    const std::int32_t dx = blobX - originX;
                    const std::int32_t dy = blobY - originY;
                    const std::int32_t dz = blobZ - originZ;
                    // parity: the squared distance is an int compared against a float radius, so the
                    // comparison happens in float and the widening is where vanilla puts it.
                    if (static_cast<float>(dx * dx + dy * dy + dz * dz) <= radiusSquared) {
                        set(chunk, blobX, blobY, blobZ, stateIdOf("mossy_cobblestone"));
                    }
                }
            }
        }
        originX += -1 + random.nextInt(2);
        originY -= random.nextInt(2);
        originZ += -1 + random.nextInt(2);
    }
}

void forestRockDecoration::decorate(Chunk& chunk, std::int64_t seed,
                                    const surface::SurfaceRuleEnvironment& environment) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kGlobalIndex, kLocalModificationsStep);
    for (std::int32_t attempt = 0; attempt < 2; ++attempt) {
        const std::int32_t localX = random.nextInt(16);
        const std::int32_t localZ = random.nextInt(16);
        // parity: `forest_rock`'s placement asks for `MOTION_BLOCKING`, not the world surface, so the
        // ferns and the grass of an old growth taiga do not lift the boulder a block.
        const std::int32_t y = chunk.heightAt(localX, localZ, HeightmapType::MotionBlocking);
        const std::int32_t worldX = chunk.chunkX() * 16 + localX;
        const std::int32_t worldZ = chunk.chunkZ() * 16 + localZ;
        if (oldGrowthTaigaBiomes().find(biomePath(environment.biomeAt(worldX, y, worldZ))) ==
            oldGrowthTaigaBiomes().end()) {
            continue;
        }
        placeAt(chunk, random, worldX, y, worldZ);
    }
}

} // namespace basalt::server::world::terrain::decoration
