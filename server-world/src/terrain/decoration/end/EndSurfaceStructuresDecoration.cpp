#include <basalt/server/random/WorldgenSeeds.hpp>
#include <basalt/server/registry/BlockStateFamilies.hpp>
#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/terrain/DensityChunkSampler.hpp>
#include <basalt/server/world/terrain/decoration/end/EndDecoration.hpp>
#include <basalt/server/world/terrain/decoration/end/EndFeatureBlocks.hpp>
#include <basalt/server/world/terrain/decoration/end/EndSpikes.hpp>
#include <basalt/wire/nbt/NbtTag.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
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
constexpr std::int32_t kReturnGatewayRarity = 700;
constexpr std::int32_t kSurfaceStructuresStep = 4;
constexpr std::int32_t kReturnGatewayIndex = 0;
constexpr std::int32_t kEndSpikeIndex = 1;

/// parity: `EndSpike.isCenterWithinChunk`'s neighbour.
[[nodiscard]] bool intersects(const EndSpike& spike, const Chunk& chunk) {
    const std::int32_t minX = chunk.chunkX() * 16;
    const std::int32_t minZ = chunk.chunkZ() * 16;
    return spike.centerX + spike.radius >= minX && spike.centerX - spike.radius <= minX + 15 &&
           spike.centerZ + spike.radius >= minZ && spike.centerZ - spike.radius <= minZ + 15;
}

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

void placeGateway(Chunk& chunk, std::int32_t originX, std::int32_t originY, std::int32_t originZ) {
    endGatewayFeature::placeBlocks(originX, originY, originZ,
                                   [&chunk](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                                       setBlock(chunk, x, y, z, stateId);
                                   });
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
    endSpikes::placeBlocks(spike, chunk.verticalBounds().minY(),
                           [&chunk](std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) {
                               setBlock(chunk, x, y, z, stateId);
                           });
    placeCrystal(chunk, spike, seed);
}

[[nodiscard]] bool intersectsTarget(const Chunk& chunk, std::int32_t x, std::int32_t z) {
    const std::int32_t minX = chunk.chunkX() * 16;
    const std::int32_t minZ = chunk.chunkZ() * 16;
    return x + 1 >= minX && x - 1 <= minX + 15 && z + 1 >= minZ && z - 1 <= minZ + 15;
}

[[nodiscard]] Chunk generateRawSourceChunk(std::int32_t chunkX, std::int32_t chunkZ, std::int64_t seed,
                                           const NoiseRouter& router, const TheEndBiomeSource& biomeSource) {
    Chunk chunk{chunkX, chunkZ, kEndHeight};
    const DensityChunkSampler sampler{router, chunkX, chunkZ};
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
                        const NoiseRouter& router, const TheEndBiomeSource& biomeSource,
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
    for (const EndSpike& spike : endSpikes::forSeed(seed)) {
        if (!intersects(spike, chunk)) {
            continue;
        }
        placeSpike(chunk, spike, seed);
        placed = true;
    }
    return placed;
}

void endSurfaceStructuresDecoration::decorate(Chunk& chunk, const std::int64_t seed, const NoiseRouter& router,
                                              const TheEndBiomeSource& biomeSource) {
    std::map<std::pair<std::int32_t, std::int32_t>, Chunk> sourceChunks;
    for (std::int32_t sourceChunkX = chunk.chunkX() - 1; sourceChunkX <= chunk.chunkX() + 1; ++sourceChunkX) {
        for (std::int32_t sourceChunkZ = chunk.chunkZ() - 1; sourceChunkZ <= chunk.chunkZ() + 1; ++sourceChunkZ) {
            placeReturnGateway(chunk, sourceChunkX, sourceChunkZ, seed, router, biomeSource, sourceChunks);
        }
    }
    for (const EndSpike& spike : endSpikes::forSeed(seed)) {
        if (intersects(spike, chunk)) {
            placeSpike(chunk, spike, seed);
        }
    }
}

} // namespace basalt::server::world::terrain::decoration::end
