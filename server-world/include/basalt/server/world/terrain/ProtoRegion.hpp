#pragma once

#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/terrain/NeighbourTerrain.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace basalt::server::world::terrain {

/// parity: `WorldGenRegion` with a block write radius of one. The chunk being decorated and its eight
/// neighbours, whose proto chunks every read and write lands in directly. Outside the 3x3 a read sees
/// nothing and a write is dropped, as vanilla drops a write into a far chunk.
class ProtoRegion final : public NeighbourTerrain {
public:
    /// `chunks` in x-major order, dx and dz from -1 to 1. A null slot is a neighbour that is already
    /// full somewhere else and is not ours to touch.
    ProtoRegion(std::int32_t centerX, std::int32_t centerZ, std::array<Chunk*, 9> chunks);

    [[nodiscard]] Chunk* chunkAt(std::int32_t chunkX, std::int32_t chunkZ) const noexcept;

    [[nodiscard]] std::optional<std::int32_t> blockStateIdAt(std::int32_t worldX, std::int32_t y,
                                                             std::int32_t worldZ) const override;

    [[nodiscard]] std::optional<std::int32_t> oceanFloorHeightAt(std::int32_t worldX,
                                                                 std::int32_t worldZ) const override;

    void setBlockStateId(std::int32_t worldX, std::int32_t y, std::int32_t worldZ, std::int32_t stateId) const;

    void setBlockEntity(std::int32_t worldX, std::int32_t y, std::int32_t worldZ, const wire::common::Identifier& type,
                        wire::nbt::NbtCompound data) const;

    /// Booked only if the block it belongs to is still the one expected.
    void scheduleBlockTick(std::int32_t worldX, std::int32_t y, std::int32_t worldZ, std::int32_t expectedStateId,
                           wire::common::Identifier type, std::int32_t delay, std::int32_t priority) const;

private:
    std::int32_t centerX_;
    std::int32_t centerZ_;
    std::array<Chunk*, 9> chunks_;
};

} // namespace basalt::server::world::terrain
