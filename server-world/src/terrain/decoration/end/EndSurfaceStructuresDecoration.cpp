#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <mutex>
#include <numbers>
#include <string_view>
#include <utility>
#include <vector>

namespace basalt::server::world::terrain::decoration::end {
namespace {

constexpr std::int32_t kEndGenerationDepth = 128;
constexpr std::int32_t kSpikeCount = 10;
constexpr double kSpikeDistance = 42.0;
constexpr std::int32_t kReturnGatewayRarity = 700;
constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kReturnGatewayIndex = 0;
constexpr std::int32_t kEndSpikeIndex = 1;

struct EndSpike {
    std::int32_t centerX = 0;
    std::int32_t centerZ = 0;
    std::int32_t radius = 0;
    std::int32_t height = 0;
    bool guarded = false;

    [[nodiscard]] bool intersects(const Chunk& chunk) const {
        const std::int32_t minX = chunk.chunkX() * 16;
        const std::int32_t minZ = chunk.chunkZ() * 16;
        return centerX + radius >= minX && centerX - radius <= minX + 15 && centerZ + radius >= minZ &&
               centerZ - radius <= minZ + 15;
    }
};

[[nodiscard]] std::int32_t stateIdOf(std::string_view path) {
    const registry::BlockState* found = registry::blocks::named(path);
    return found == nullptr ? registry::blocks::air().stateId : found->stateId;
}

void setBlock(Chunk& chunk, std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
    if ((x >> 4) != chunk.chunkX() || (z >> 4) != chunk.chunkZ()) {
        return;
    }
    const ChunkHeight bounds = chunk.verticalBounds();
    if (y < bounds.minY() || y > bounds.maxY()) {
        return;
    }
    chunk.setBlockStateId(x & 15, y, z & 15, stateId);
}

/// parity: `SpikeConfiguration`'s own shuffle of the ten sizes, seeded from the world seed's first
/// legacy draw.
[[nodiscard]] const std::vector<EndSpike>& spikesForSeed(std::int64_t seed) {
    // Every End chunk asks for these, so several generation workers reach the cache at once. The
    // reference outlives the lock because a map keeps its nodes where they are and nothing erases.
    static std::mutex mutex;
    static std::map<std::int64_t, std::vector<EndSpike>> cache;
    const std::scoped_lock lock{mutex};
    const auto found = cache.find(seed);
    if (found != cache.end()) {
        return found->second;
    }
    random::RandomSource keySource = random::RandomSource::legacy(seed);
    const std::int64_t key = keySource.nextLong() & 65535LL;
    std::vector<std::int32_t> sizes(kSpikeCount);
    for (std::int32_t index = 0; index < kSpikeCount; ++index) {
        sizes[static_cast<std::size_t>(index)] = index;
    }
    random::RandomSource random = random::RandomSource::legacy(key);
    for (std::int32_t size = kSpikeCount; size >= 2; --size) {
        const std::int32_t swapTo = random.nextInt(size);
        std::swap(sizes[static_cast<std::size_t>(size - 1)], sizes[static_cast<std::size_t>(swapTo)]);
    }
    std::vector<EndSpike> spikes;
    spikes.reserve(kSpikeCount);
    for (std::int32_t index = 0; index < kSpikeCount; ++index) {
        const double angle = 2.0 * (-std::numbers::pi + std::numbers::pi / kSpikeCount * index);
        const std::int32_t size = sizes[static_cast<std::size_t>(index)];
        spikes.push_back(EndSpike{.centerX = static_cast<std::int32_t>(std::floor(kSpikeDistance * std::cos(angle))),
                                  .centerZ = static_cast<std::int32_t>(std::floor(kSpikeDistance * std::sin(angle))),
                                  .radius = 2 + size / 3,
                                  .height = 76 + size * 3,
                                  .guarded = size == 1 || size == 2});
    }
    return cache.emplace(seed, std::move(spikes)).first->second;
}

void placeGateway(Chunk& chunk, std::int32_t originX, std::int32_t originY, std::int32_t originZ) {
    for (std::int32_t z = originZ - 1; z <= originZ + 1; ++z) {
        for (std::int32_t y = originY - 2; y <= originY + 2; ++y) {
            for (std::int32_t x = originX - 1; x <= originX + 1; ++x) {
                const bool sameX = x == originX;
                const bool sameY = y == originY;
                const bool sameZ = z == originZ;
                const bool end = std::abs(y - originY) == 2;
                std::int32_t stateId = registry::blocks::air().stateId;
                if (sameX && sameY && sameZ) {
                    stateId = stateIdOf("end_gateway");
                } else if (sameY) {
                    stateId = registry::blocks::air().stateId;
                } else if (end && sameX && sameZ) {
                    stateId = stateIdOf("bedrock");
                } else if ((sameX || sameZ) && !end) {
                    stateId = stateIdOf("bedrock");
                }
                setBlock(chunk, x, y, z, stateId);
            }
        }
    }
    if ((originX >> 4) == chunk.chunkX() && (originZ >> 4) == chunk.chunkZ()) {
        wire::nbt::NbtCompound data;
        data.putString("id", "minecraft:end_gateway");
        data.putLong("Age", 0LL);
        data.putIntArray("exit_portal", {100, 50, 0});
        data.putBoolean("ExactTeleport", true);
        data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
        chunk.setBlockEntity(ChunkBlockEntity{originX & 15, originY, originZ & 15,
                                              wire::common::Identifier::vanilla("end_gateway"), std::move(data)});
    }
}

void placeCage(Chunk& chunk, const EndSpike& spike) {
    for (std::int32_t dx = -2; dx <= 2; ++dx) {
        for (std::int32_t dz = -2; dz <= 2; ++dz) {
            for (std::int32_t dy = 0; dy <= 3; ++dy) {
                const bool isXSide = std::abs(dx) == 2;
                const bool isZSide = std::abs(dz) == 2;
                const bool top = dy == 3;
                if (!isXSide && !isZSide && !top) {
                    continue;
                }
                const bool xEdge = dx == -2 || dx == 2 || top;
                const bool zEdge = dz == -2 || dz == 2 || top;
                const std::int32_t stateId = registry::blockStateFamilies::ironBarsStateId(
                    xEdge && dz != -2, zEdge && dx != 2, xEdge && dz != 2, zEdge && dx != -2);
                setBlock(chunk, spike.centerX + dx, spike.height + dy, spike.centerZ + dz, stateId);
            }
        }
    }
}

void placeCrystal(Chunk& chunk, const EndSpike& spike, std::int64_t seed) {
    if ((spike.centerX >> 4) != chunk.chunkX() || (spike.centerZ >> 4) != chunk.chunkZ()) {
        return;
    }
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed =
        random::setDecorationSeed(random, seed, (spike.centerX >> 4) * 16, (spike.centerZ >> 4) * 16);
    random::setFeatureSeed(random, decorationSeed, kEndSpikeIndex, kSurfaceStructuresStep);
    chunk.addGeneratedEntity(ChunkGeneratedEntity{.type = wire::common::Identifier::vanilla("end_crystal"),
                                                  .x = spike.centerX + 0.5,
                                                  .y = spike.height + 1.0,
                                                  .z = spike.centerZ + 0.5,
                                                  .yaw = random.nextFloat() * 360.0F,
                                                  .pitch = 0.0F});
    setBlock(chunk, spike.centerX, spike.height, spike.centerZ, stateIdOf("bedrock"));
    setBlock(chunk, spike.centerX, spike.height + 1, spike.centerZ, stateIdOf("fire"));
}

void placeSpike(Chunk& chunk, const EndSpike& spike, std::int64_t seed) {
    const ChunkHeight bounds = chunk.verticalBounds();
    const std::int32_t minX = std::max(chunk.chunkX() * 16, spike.centerX - spike.radius);
    const std::int32_t maxX = std::min(chunk.chunkX() * 16 + 15, spike.centerX + spike.radius);
    const std::int32_t minZ = std::max(chunk.chunkZ() * 16, spike.centerZ - spike.radius);
    const std::int32_t maxZ = std::min(chunk.chunkZ() * 16 + 15, spike.centerZ + spike.radius);
    const std::int32_t obsidian = stateIdOf("obsidian");
    const std::int32_t air = registry::blocks::air().stateId;
    for (std::int32_t z = minZ; z <= maxZ; ++z) {
        for (std::int32_t x = minX; x <= maxX; ++x) {
            const std::int32_t dx = x - spike.centerX;
            const std::int32_t dz = z - spike.centerZ;
            const bool insidePillar = dx * dx + dz * dz <= spike.radius * spike.radius + 1;
            for (std::int32_t y = bounds.minY(); y <= spike.height + 10; ++y) {
                if (insidePillar && y < spike.height) {
                    setBlock(chunk, x, y, z, obsidian);
                } else if (y > 65) {
                    setBlock(chunk, x, y, z, air);
                }
            }
        }
    }
    if (spike.guarded) {
        placeCage(chunk, spike);
    }
    placeCrystal(chunk, spike, seed);
}

[[nodiscard]] bool intersectsTarget(const Chunk& chunk, std::int32_t x, std::int32_t z) {
    const std::int32_t minX = chunk.chunkX() * 16;
    const std::int32_t minZ = chunk.chunkZ() * 16;
    return x + 1 >= minX && x - 1 <= minX + 15 && z + 1 >= minZ && z - 1 <= minZ + 15;
}

/// A gateway can be rolled in a neighbouring chunk and still reach into this one, and its height
/// comes from that chunk's own terrain, so the neighbour has to be generated to ask.
[[nodiscard]] Chunk generateRawSourceChunk(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                           const EndNoiseRouter& router, const TheEndBiomeSource& biomeSource) {
    Chunk chunk{chunkX, chunkZ, kEndHeight};
    DensityChunkSampler sampler{router.finalDensity, chunkX, chunkZ, 0, kEndGenerationDepth, 8, 4};
    const std::int32_t endStone = stateIdOf("end_stone");
    for (std::int32_t y = 0; y < kEndGenerationDepth; ++y) {
        for (std::int32_t z = 0; z < 16; ++z) {
            for (std::int32_t x = 0; x < 16; ++x) {
                if (sampler.sample(x, y, z) > 0.0) {
                    chunk.setBlockStateId(x, y, z, endStone);
                }
            }
        }
    }
    endRawDecoration::decorate(chunk, seed, biomeSource);
    return chunk;
}

void placeReturnGateway(Chunk& target, std::int32_t sourceChunkX, std::int32_t sourceChunkZ, std::int64_t seed,
                        const EndNoiseRouter& router, const TheEndBiomeSource& biomeSource,
                        std::map<std::pair<std::int32_t, std::int32_t>, Chunk>& sourceChunks) {
    random::RandomSource random = random::RandomSource::worldgen(random::RandomSource::xoroshiro(seed));
    const std::int64_t decorationSeed = random::setDecorationSeed(random, seed, sourceChunkX * 16, sourceChunkZ * 16);
    random::setFeatureSeed(random, decorationSeed, kReturnGatewayIndex, kSurfaceStructuresStep);
    if (random.nextFloat() >= 1.0F / static_cast<float>(kReturnGatewayRarity)) {
        return;
    }
    const std::int32_t x = sourceChunkX * 16 + random.nextInt(16);
    const std::int32_t z = sourceChunkZ * 16 + random.nextInt(16);
    if (!intersectsTarget(target, x, z)) {
        return;
    }
    const Chunk* source = &target;
    if (sourceChunkX != target.chunkX() || sourceChunkZ != target.chunkZ()) {
        const std::pair<std::int32_t, std::int32_t> key{sourceChunkX, sourceChunkZ};
        auto cached = sourceChunks.find(key);
        if (cached == sourceChunks.end()) {
            cached =
                sourceChunks.emplace(key, generateRawSourceChunk(sourceChunkX, sourceChunkZ, seed, router, biomeSource))
                    .first;
        }
        source = &cached->second;
    }
    const std::int32_t surfaceY = source->heightAt(x & 15, z & 15, HeightmapType::MotionBlocking);
    if (surfaceY <= source->verticalBounds().minY()) {
        return;
    }
    const std::int32_t y = surfaceY + 3 + random.nextInt(7);
    if (biomeSource.blockBiomeAt(x, y, z) != "minecraft:end_highlands") {
        return;
    }
    placeGateway(target, x, y, z);
}

} // namespace

bool endSurfaceStructuresDecoration::placeSpikesIn(Chunk& chunk, const std::int64_t seed) {
    bool placed = false;
    for (const EndSpike& spike : spikesForSeed(seed)) {
        if (!spike.intersects(chunk)) {
            continue;
        }
        placeSpike(chunk, spike, seed);
        placed = true;
    }
    return placed;
}

void endSurfaceStructuresDecoration::decorate(Chunk& chunk, const std::int64_t seed, const EndNoiseRouter& router,
                                              const TheEndBiomeSource& biomeSource) {
    std::map<std::pair<std::int32_t, std::int32_t>, Chunk> sourceChunks;
    for (std::int32_t sourceChunkX = chunk.chunkX() - 1; sourceChunkX <= chunk.chunkX() + 1; ++sourceChunkX) {
        for (std::int32_t sourceChunkZ = chunk.chunkZ() - 1; sourceChunkZ <= chunk.chunkZ() + 1; ++sourceChunkZ) {
            placeReturnGateway(chunk, sourceChunkX, sourceChunkZ, seed, router, biomeSource, sourceChunks);
        }
    }
    for (const EndSpike& spike : spikesForSeed(seed)) {
        if (spike.intersects(chunk)) {
            placeSpike(chunk, spike, seed);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::end
