#include <basalt/server/world/terrain/ProtoRegion.hpp>

#include <cstdint>
#include <utility>

namespace basalt::server::world::terrain {

ProtoRegion::ProtoRegion(const std::int32_t centerX, const std::int32_t centerZ, const std::array<Chunk*, 9> chunks)
    : centerX_(centerX), centerZ_(centerZ), chunks_(chunks) {}

Chunk* ProtoRegion::chunkAt(const std::int32_t chunkX, const std::int32_t chunkZ) const noexcept {
    const std::int32_t dx = chunkX - centerX_;
    const std::int32_t dz = chunkZ - centerZ_;
    if (dx < -1 || dx > 1 || dz < -1 || dz > 1) {
        return nullptr;
    }
    return chunks_[static_cast<std::size_t>(((dx + 1) * 3) + dz + 1)];
}

std::optional<std::int32_t> ProtoRegion::blockStateIdAt(const std::int32_t worldX, const std::int32_t y,
                                                        const std::int32_t worldZ) const {
    const Chunk* chunk = chunkAt(worldX >> 4, worldZ >> 4);
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return std::nullopt;
    }
    return chunk->blockStateId(worldX & 15, y, worldZ & 15);
}

std::optional<std::int32_t> ProtoRegion::oceanFloorHeightAt(const std::int32_t worldX,
                                                            const std::int32_t worldZ) const {
    const Chunk* chunk = chunkAt(worldX >> 4, worldZ >> 4);
    if (chunk == nullptr) {
        return std::nullopt;
    }
    return chunk->worldgenHeightAt(worldX & 15, worldZ & 15, HeightmapType::OceanFloor);
}

void ProtoRegion::setBlockStateId(const std::int32_t worldX, const std::int32_t y, const std::int32_t worldZ,
                                  const std::int32_t stateId) const {
    Chunk* chunk = chunkAt(worldX >> 4, worldZ >> 4);
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return;
    }
    chunk->setBlockStateId(worldX & 15, y, worldZ & 15, stateId);
}

void ProtoRegion::setBlockEntity(const std::int32_t worldX, const std::int32_t y, const std::int32_t worldZ,
                                 const wire::common::Identifier& type, wire::nbt::NbtCompound data) const {
    Chunk* chunk = chunkAt(worldX >> 4, worldZ >> 4);
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return;
    }
    chunk->setBlockEntity(ChunkBlockEntity{worldX & 15, y, worldZ & 15, type, std::move(data)});
}

void ProtoRegion::scheduleBlockTick(const std::int32_t worldX, const std::int32_t y, const std::int32_t worldZ,
                                    const std::int32_t expectedStateId, wire::common::Identifier type,
                                    const std::int32_t delay, const std::int32_t priority) const {
    Chunk* chunk = chunkAt(worldX >> 4, worldZ >> 4);
    if (chunk == nullptr || y < chunk->verticalBounds().minY() || y > chunk->verticalBounds().maxY()) {
        return;
    }
    if (chunk->blockStateId(worldX & 15, y, worldZ & 15) != expectedStateId) {
        return;
    }
    chunk->scheduleBlockTick(ChunkScheduledTick{std::move(type), worldX & 15, y, worldZ & 15, delay, priority});
}

} // namespace basalt::server::world::terrain
