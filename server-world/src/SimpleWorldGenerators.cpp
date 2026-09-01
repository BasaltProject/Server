#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/SimpleWorldGenerators.hpp>

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
    // Only the chunks the platform actually reaches do any work; everything else stays air.
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

} // namespace basalt::server::world
