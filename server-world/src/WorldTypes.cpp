#include <basalt/server/registry/Blocks.hpp>
#include <basalt/server/world/WorldTypes.hpp>

#include <cstdint>

namespace basalt::server::world {

Chunk flatWorld::generate(std::int32_t chunkX, std::int32_t chunkZ, ChunkHeight bounds) {
    Chunk chunk{chunkX, chunkZ, bounds};
    const std::int32_t minY = bounds.minY();
    const std::int32_t dirt = registry::blocks::named("dirt")->stateId;
    const std::int32_t grass = registry::blocks::named("grass_block")->stateId;
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            chunk.setBlockStateId(x, minY, z, registry::blocks::bedrock().stateId);
            chunk.setBlockStateId(x, minY + 1, z, dirt);
            chunk.setBlockStateId(x, minY + 2, z, dirt);
            chunk.setBlockStateId(x, minY + 3, z, grass);
        }
    }
    return chunk;
}

Chunk voidWorld::generate(std::int32_t chunkX, std::int32_t chunkZ, std::int32_t spawnX, std::int32_t spawnY,
                          std::int32_t spawnZ, std::int32_t platformRadius, ChunkHeight bounds) {
    Chunk chunk{chunkX, chunkZ, bounds};
    if (platformRadius <= 0) {
        return chunk;
    }
    for (std::int32_t x = 0; x < 16; ++x) {
        for (std::int32_t z = 0; z < 16; ++z) {
            const std::int32_t worldX = chunkX * 16 + x;
            const std::int32_t worldZ = chunkZ * 16 + z;
            if (worldX < spawnX - platformRadius || worldX > spawnX + platformRadius) {
                continue;
            }
            if (worldZ < spawnZ - platformRadius || worldZ > spawnZ + platformRadius) {
                continue;
            }
            chunk.setBlockStateId(x, spawnY - 1, z, registry::blocks::stone().stateId);
        }
    }
    return chunk;
}

} // namespace basalt::server::world
