#include <basalt/server/registry/BlockFluids.hpp>
#include <basalt/server/registry/BlockShapes.hpp>
#include <basalt/server/runtime/world/WorldSpawnPoint.hpp>

#include <cstdint>

namespace basalt::server::runtime::world::worldSpawnPoint {
namespace {

namespace reg = server::registry;

using server::world::Chunk;
using server::world::HeightmapType;

/// parity: `PlayerSpawnFinder.getLevelRespawnPos`.
[[nodiscard]] std::optional<std::array<std::int32_t, 3>> respawnPos(const Chunk& chunk, std::int32_t localX,
                                                                    std::int32_t localZ) {
    const std::int32_t minY = chunk.verticalBounds().minY();
    const std::int32_t topY = chunk.heightAt(localX, localZ, HeightmapType::MotionBlocking);
    if (topY < minY) {
        return std::nullopt;
    }
    const std::int32_t surface = chunk.heightAt(localX, localZ, HeightmapType::WorldSurface);
    if (surface <= topY && surface > chunk.heightAt(localX, localZ, HeightmapType::OceanFloor)) {
        return std::nullopt;
    }
    const std::int32_t worldX = (chunk.chunkX() * 16) + localX;
    const std::int32_t worldZ = (chunk.chunkZ() * 16) + localZ;
    for (std::int32_t y = topY + 1; y >= minY; y--) {
        const std::int32_t state = chunk.blockStateId(localX, y, localZ);
        if (reg::blockFluids::hasFluid(state)) {
            return std::nullopt;
        }
        if (reg::blockShapes::isCollisionFaceFull(state, reg::BlockFace::Up)) {
            return std::array<std::int32_t, 3>{worldX, y + 1, worldZ};
        }
    }
    return std::nullopt;
}

} // namespace

std::optional<std::array<std::int32_t, 3>> spawnInChunk(const Chunk& chunk) {
    for (std::int32_t localX = 0; localX < 16; localX++) {
        for (std::int32_t localZ = 0; localZ < 16; localZ++) {
            if (std::optional<std::array<std::int32_t, 3>> found = respawnPos(chunk, localX, localZ);
                found.has_value()) {
                return found;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::array<std::int32_t, 3>> find(std::int32_t spawnChunkX, std::int32_t spawnChunkZ,
                                                const ChunkAt& chunkAt) {
    if (!chunkAt) {
        return std::nullopt;
    }
    std::int32_t offsetX = 0;
    std::int32_t offsetZ = 0;
    std::int32_t stepX = 0;
    std::int32_t stepZ = -1;
    for (std::int32_t step = 0; step < kSpiralChunks * kSpiralChunks; step++) {
        if (offsetX >= -kReach && offsetX <= kReach && offsetZ >= -kReach && offsetZ <= kReach) {
            if (std::optional<std::array<std::int32_t, 3>> found =
                    spawnInChunk(chunkAt(spawnChunkX + offsetX, spawnChunkZ + offsetZ));
                found.has_value()) {
                return found;
            }
        }
        // parity: the square spiral of `setInitialSpawn`, which turns at the diagonals.
        const bool turns =
            offsetX == offsetZ || (offsetX < 0 && offsetX == -offsetZ) || (offsetX > 0 && offsetX == 1 - offsetZ);
        if (turns) {
            const std::int32_t turned = stepX;
            stepX = -stepZ;
            stepZ = turned;
        }
        offsetX += stepX;
        offsetZ += stepZ;
    }
    return std::nullopt;
}

} // namespace basalt::server::runtime::world::worldSpawnPoint
