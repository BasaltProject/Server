#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherLocalDecoration.hpp>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace basalt::server::world::terrain::decoration::nether {
namespace {

constexpr std::int32_t kLocalModificationsStep = 2;
constexpr std::int32_t kBasaltPillarIndex = 0;

/// parity: the order the 26.3 `basalt_pillar` lists its sides in: east, west, south, north.
constexpr std::array<std::pair<std::int32_t, std::int32_t>, 4> kSides{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

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

[[nodiscard]] bool isAir(const Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return false;
    }
    return registry::blocks::isAir(chunk.decorationBlockStateId(x, y, z));
}

void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y >= bounds.minY() && y <= bounds.maxY()) {
        chunk.setDecorationBlockStateId(x, y, z, stateId);
    }
}

/// parity: `SingleBlockPillarFeature` over air: each step rolls its chance even when it is certain,
/// and the pillar stops at the first block that is not air. Returns the last block it placed.
std::int32_t placeColumnDown(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                             float chance, std::int32_t basalt) {
    const ChunkHeight bounds = chunk.verticalBounds();
    // parity: outside the build height the level reads void air, which the air test accepts.
    const auto outside = [&](std::int32_t level) { return level < bounds.minY() || level > bounds.maxY(); };
    while ((outside(y) || isAir(chunk, x, y, z)) && random.nextFloat() < chance && !outside(y)) {
        setBlock(chunk, x, y, z, basalt);
        --y;
    }
    return y + 1;
}

/// parity: `ProjectedRandomPatchySquare` of size 3, each kept cell dropped through up to three air
/// blocks and turned to basalt only where something solid is below.
void placeBase(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
               std::int32_t originZ, std::int32_t basalt) {
    for (std::int32_t offsetX = -3; offsetX <= 3; ++offsetX) {
        for (std::int32_t offsetZ = -3; offsetZ <= 3; ++offsetZ) {
            const std::int32_t probability = std::abs(offsetX) * std::abs(offsetZ);
            if (random.nextInt(10) >= 10 - probability) {
                continue;
            }
            const std::int32_t x = originX + offsetX;
            const std::int32_t z = originZ + offsetZ;
            std::int32_t y = originY;
            std::int32_t maxDrop = 3;
            while (isAir(chunk, x, y - 1, z)) {
                y--;
                if (--maxDrop <= 0) {
                    break;
                }
            }
            if (!isAir(chunk, x, y - 1, z)) {
                setBlock(chunk, x, y, z, basalt);
            }
        }
    }
}

/// parity: the 26.3 `basalt_pillar`, an `overlay` of a capped pillar and four side pillars. The cap
/// is the patchy base under the pillar's foot and four `rarity_filter` basalt blocks beside it.
void placePillar(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                 std::int32_t originZ) {
    if (!isAir(chunk, originX, originY, originZ) || isAir(chunk, originX, originY + 1, originZ)) {
        return;
    }
    const std::int32_t basalt = stateIdOf("basalt");
    const std::int32_t foot = placeColumnDown(chunk, random, originX, originY, originZ, 1.0F, basalt);
    placeBase(chunk, random, originX, foot - 1, originZ, basalt);
    for (const auto [dx, dz] : kSides) {
        if (random.nextFloat() < 0.5F) {
            setBlock(chunk, originX + dx, foot, originZ + dz, basalt);
        }
    }
    for (const auto [dx, dz] : kSides) {
        static_cast<void>(placeColumnDown(chunk, random, originX + dx, originY, originZ + dz, 0.9F, basalt));
    }
}

} // namespace

void netherLocalDecoration::placeAt(Chunk& chunk, random::RandomSource& random, const std::int32_t x,
                                    const std::int32_t y, const std::int32_t z) {
    placePillar(chunk, random, x, y, z);
}

void netherLocalDecoration::decorate(Chunk& chunk, const std::int64_t seed, const NetherBiomeLookup& biomeSource) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, chunk.chunkX() * 16, chunk.chunkZ() * 16);
    random::setFeatureSeed(random, decorationSeed, kBasaltPillarIndex, kLocalModificationsStep);
    for (std::int32_t attempt = 0; attempt < 10; ++attempt) {
        const std::int32_t x = chunk.chunkX() * 16 + random.nextInt(16);
        const std::int32_t z = chunk.chunkZ() * 16 + random.nextInt(16);
        const std::int32_t y = random.nextInt(128);
        if (biomePath(biomeSource(x, y, z, nullptr)) == "soul_sand_valley") {
            placePillar(chunk, random, x, y, z);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::nether
