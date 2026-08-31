#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/decoration/nether/NetherLocalDecoration.hpp>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

namespace basalt::server::world::terrain::decoration::nether {
namespace {

constexpr std::int32_t kLocalModificationsStep = 2;
constexpr std::int32_t kBasaltPillarIndex = 0;

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

/// A side block that stops being placed once one roll fails, which is what tapers the pillar.
[[nodiscard]] bool placeHangOff(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y,
                                std::int32_t z, std::int32_t basalt) {
    if (random.nextInt(10) == 0) {
        return false;
    }
    setBlock(chunk, x, y, z, basalt);
    return true;
}

void placeBaseHangOff(Chunk& chunk, random::RandomSource& random, std::int32_t x, std::int32_t y, std::int32_t z,
                      std::int32_t basalt) {
    if (random.nextBoolean()) {
        setBlock(chunk, x, y, z, basalt);
    }
}

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

void placePillar(Chunk& chunk, random::RandomSource& random, std::int32_t originX, std::int32_t originY,
                 std::int32_t originZ) {
    if (!isAir(chunk, originX, originY, originZ) || isAir(chunk, originX, originY + 1, originZ)) {
        return;
    }
    const std::int32_t basalt = stateIdOf("basalt");
    const ChunkHeight bounds = chunk.verticalBounds();
    std::int32_t y = originY;
    bool placeNorth = true;
    bool placeSouth = true;
    bool placeWest = true;
    bool placeEast = true;

    while (true) {
        if (y < bounds.minY() || y > bounds.maxY()) {
            return;
        }
        if (!isAir(chunk, originX, y, originZ)) {
            break;
        }
        setBlock(chunk, originX, y, originZ, basalt);
        placeNorth = placeNorth && placeHangOff(chunk, random, originX, y, originZ - 1, basalt);
        placeSouth = placeSouth && placeHangOff(chunk, random, originX, y, originZ + 1, basalt);
        placeWest = placeWest && placeHangOff(chunk, random, originX - 1, y, originZ, basalt);
        placeEast = placeEast && placeHangOff(chunk, random, originX + 1, y, originZ, basalt);
        y--;
    }

    const std::int32_t lastPillarY = y + 1;
    placeBaseHangOff(chunk, random, originX, lastPillarY, originZ - 1, basalt);
    placeBaseHangOff(chunk, random, originX, lastPillarY, originZ + 1, basalt);
    placeBaseHangOff(chunk, random, originX - 1, lastPillarY, originZ, basalt);
    placeBaseHangOff(chunk, random, originX + 1, lastPillarY, originZ, basalt);
    placeBase(chunk, random, originX, y, originZ, basalt);
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
