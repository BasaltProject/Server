#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/SimpleWorldGenerators.hpp>

#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace basalt::server::world {

Chunk flatWorldGenerator::generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight verticalBounds) {
    Chunk chunk{chunkX, chunkZ, verticalBounds};
    const std::int32_t minY = verticalBounds.minY();
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, minY, z, registry::blocks::named("bedrock")->stateId);
            chunk.setBlockStateId(x, minY + 1, z, registry::blocks::named("dirt")->stateId);
            chunk.setBlockStateId(x, minY + 2, z, registry::blocks::named("dirt")->stateId);
            chunk.setBlockStateId(x, minY + 3, z, registry::blocks::named("grass_block")->stateId);
        }
    }
    return chunk;
}

Chunk voidWorldGenerator::generate(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t spawnX, std::int32_t spawnY,
                                   std::int32_t spawnZ, std::int32_t platformRadius, ChunkHeight verticalBounds) {
    Chunk chunk{chunkX, chunkZ, verticalBounds};
    if (platformRadius <= 0) {
        return chunk;
    }
    const std::int32_t stone = registry::blocks::named("stone")->stateId;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = (chunkX * 16) + x;
            const std::int32_t worldZ = (chunkZ * 16) + z;
            if (worldX < spawnX - platformRadius || worldX > spawnX + platformRadius) {
                continue;
            }
            if (worldZ < spawnZ - platformRadius || worldZ > spawnZ + platformRadius) {
                continue;
            }
            chunk.setBlockStateId(x, spawnY - 1, z, stone);
        }
    }
    return chunk;
}

std::int32_t debugWorldGenerator::stateAt(std::int32_t worldX, std::int32_t worldZ) {
    const std::int32_t count = registry::blocks::stateCount();
    // parity: `Mth.ceil(Mth.sqrt(float))`, in float as the jar does it.
    const auto width = static_cast<std::int32_t>(std::ceil(std::sqrt(static_cast<float>(count))));
    const auto height = static_cast<std::int32_t>(std::ceil(static_cast<float>(count) / static_cast<float>(width)));
    if (worldX <= 0 || worldZ <= 0 || worldX % 2 == 0 || worldZ % 2 == 0) {
        return registry::blocks::air().stateId;
    }
    const std::int32_t gridX = worldX / 2;
    const std::int32_t gridZ = worldZ / 2;
    if (gridX > width || gridZ > height) {
        return registry::blocks::air().stateId;
    }
    const std::int32_t index = std::abs((gridX * width) + gridZ);
    return index < count ? index : registry::blocks::air().stateId;
}

Chunk debugWorldGenerator::generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight verticalBounds) {
    Chunk chunk{chunkX, chunkZ, verticalBounds};
    const std::int32_t barrier = registry::blocks::named("barrier")->stateId;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, kBarrierY, z, barrier);
            chunk.setBlockStateId(x, kStateY, z, stateAt((chunkX * 16) + x, (chunkZ * 16) + z));
        }
    }
    return chunk;
}

} // namespace basalt::server::world
