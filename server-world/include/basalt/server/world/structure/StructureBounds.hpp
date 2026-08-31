#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace basalt::server::world::structure {

/// A block position, in world coordinates.
struct StructurePos {
    std::int32_t x = 0;
    std::int32_t y = 0;
    std::int32_t z = 0;

    friend bool operator==(const StructurePos&, const StructurePos&) = default;
};

/// parity: `net.minecraft.world.level.levelgen.structure.BoundingBox`, axis-aligned and inclusive on
/// every bound.
struct StructureBoundingBox {
    std::int32_t minX = 0;
    std::int32_t minY = 0;
    std::int32_t minZ = 0;
    std::int32_t maxX = 0;
    std::int32_t maxY = 0;
    std::int32_t maxZ = 0;

    [[nodiscard]] constexpr bool isInside(StructurePos pos) const noexcept {
        return pos.x >= minX && pos.x <= maxX && pos.y >= minY && pos.y <= maxY && pos.z >= minZ && pos.z <= maxZ;
    }

    [[nodiscard]] constexpr StructureBoundingBox move(StructurePos offset) const noexcept {
        return {minX + offset.x, minY + offset.y, minZ + offset.z, maxX + offset.x, maxY + offset.y, maxZ + offset.z};
    }

    /// Whether `other` is entirely within this box on every axis, not merely overlapping.
    [[nodiscard]] constexpr bool containsFully(const StructureBoundingBox& other) const noexcept {
        return minX <= other.minX && other.maxX <= maxX && minY <= other.minY && other.maxY <= maxY &&
               minZ <= other.minZ && other.maxZ <= maxZ;
    }

    [[nodiscard]] constexpr bool intersects(const StructureBoundingBox& other) const noexcept {
        return minX <= other.maxX && maxX >= other.minX && minZ <= other.maxZ && maxZ >= other.minZ &&
               minY <= other.maxY && maxY >= other.minY;
    }

    [[nodiscard]] static constexpr StructureBoundingBox fromCorners(StructurePos first, StructurePos second) noexcept {
        return {std::min(first.x, second.x), std::min(first.y, second.y), std::min(first.z, second.z),
                std::max(first.x, second.x), std::max(first.y, second.y), std::max(first.z, second.z)};
    }

    friend bool operator==(const StructureBoundingBox&, const StructureBoundingBox&) = default;
};

/// One structure start: where it belongs, the union of its pieces, and each piece's own box.
struct StructureStartRecord {
    /// The structure's registry path, without the namespace, as `fortress` or `ancient_city`.
    std::string structure;
    /// The chunk the start belongs to, which is what a reference points at.
    std::int32_t chunkX = 0;
    std::int32_t chunkZ = 0;
    /// parity: `StructureStart.getBoundingBox`, the union of every piece.
    StructureBoundingBox fullBounds;
    /// parity: `StructureStart.getPieces`, each piece's own box.
    std::vector<StructureBoundingBox> pieces;

    /// parity: `StructureManager.structureHasPieceAt`.
    [[nodiscard]] bool hasPieceAt(StructurePos pos) const;

    /// Whether `pos` falls in the start as a whole. parity: `getStructureAt`.
    [[nodiscard]] bool contains(StructurePos pos) const { return fullBounds.isInside(pos); }

    /// Builds a start from its pieces, taking the full bounds as their union. Absent for no pieces.
    [[nodiscard]] static std::optional<StructureStartRecord>
    of(std::string structure, std::int32_t chunkX, std::int32_t chunkZ, std::vector<StructureBoundingBox> pieces);

    friend bool operator==(const StructureStartRecord&, const StructureStartRecord&) = default;
};

/// The structure starts that reach into one chunk, sliced to the pieces that touch it: a spawn query
/// must not load a chunk or read one another region owns. The full bounds travel with every slice.
class ChunkStructures {
public:
    [[nodiscard]] bool empty() const noexcept { return starts_.empty(); }

    [[nodiscard]] std::span<const StructureStartRecord> starts() const noexcept { return starts_; }

    /// Whether any start of `structure` reaches this chunk, the cheap gate before a box test.
    [[nodiscard]] bool has(std::string_view structure) const;

    /// Whether it was added: a chunk is generated once, but a start can be offered again by a
    /// neighbour's pass, and the caller marks the chunk dirty only for one that was new.
    bool add(StructureStartRecord start);

    /// Restores a decoded chunk without marking it dirty again.
    void restore(std::vector<StructureStartRecord> starts);

private:
    std::vector<StructureStartRecord> starts_;
};

} // namespace basalt::server::world::structure
