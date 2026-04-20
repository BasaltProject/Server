#pragma once

#include <basalt/server/world/terrain/NetherFortressPieces.hpp>

#include <cstdint>
#include <initializer_list>
#include <map>
#include <string>

namespace basalt::server::world::terrain::netherFortressPieces {

/// parity: the shared per-piece geometry and block helpers of `NetherFortressPieces`.
class Brush {
public:
    Brush(const Piece& piece, Chunk& chunk, const structure::StructureBoundingBox& clip)
        : piece_(&piece), chunk_(&chunk), clip_(clip) {}

    [[nodiscard]] const Piece& piece() const noexcept { return *piece_; }

    [[nodiscard]] Chunk& chunk() const noexcept { return *chunk_; }

    [[nodiscard]] const structure::StructureBoundingBox& clip() const noexcept { return clip_; }

    [[nodiscard]] std::int32_t worldX(std::int32_t x, std::int32_t z) const;
    [[nodiscard]] std::int32_t worldZ(std::int32_t x, std::int32_t z) const;

    [[nodiscard]] structure::StructureDirection transformDirection(structure::StructureDirection direction) const;

    [[nodiscard]] std::int32_t fence(std::initializer_list<structure::StructureDirection> connections) const;
    [[nodiscard]] std::int32_t stairs(structure::StructureDirection facing) const;

    void box(std::int32_t x0, std::int32_t y0, std::int32_t z0, std::int32_t x1, std::int32_t y1, std::int32_t z1,
             std::int32_t stateId) const;

    void placeBlock(std::int32_t x, std::int32_t y, std::int32_t z, std::int32_t stateId) const;

    void fillDown(std::int32_t x, std::int32_t startY, std::int32_t z, std::int32_t stateId) const;

    [[nodiscard]] bool isInside(std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] std::int32_t blockAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

    [[nodiscard]] bool isChestAt(std::int32_t x, std::int32_t y, std::int32_t z) const;

private:
    const Piece* piece_;
    Chunk* chunk_;
    structure::StructureBoundingBox clip_;
};

[[nodiscard]] std::int32_t stateOf(std::string_view path, const std::map<std::string, std::string>& properties);

} // namespace basalt::server::world::terrain::netherFortressPieces
