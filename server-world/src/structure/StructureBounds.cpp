#include <basalt/server/world/structure/StructureBounds.hpp>

#include <algorithm>
#include <utility>

namespace basalt::server::world::structure {

bool StructureStartRecord::hasPieceAt(StructurePos pos) const {
    return std::ranges::any_of(pieces, [pos](const StructureBoundingBox& piece) { return piece.isInside(pos); });
}

std::optional<StructureStartRecord> StructureStartRecord::of(std::string structure, std::int32_t chunkX,
                                                             std::int32_t chunkZ,
                                                             std::vector<StructureBoundingBox> pieces) {
    if (pieces.empty()) {
        return std::nullopt;
    }
    StructureBoundingBox box = pieces.front();
    for (const StructureBoundingBox& piece : pieces) {
        box.minX = std::min(box.minX, piece.minX);
        box.minY = std::min(box.minY, piece.minY);
        box.minZ = std::min(box.minZ, piece.minZ);
        box.maxX = std::max(box.maxX, piece.maxX);
        box.maxY = std::max(box.maxY, piece.maxY);
        box.maxZ = std::max(box.maxZ, piece.maxZ);
    }
    return StructureStartRecord{std::move(structure), chunkX, chunkZ, box, std::move(pieces)};
}

bool ChunkStructures::has(std::string_view structure) const {
    return std::ranges::any_of(starts_,
                               [structure](const StructureStartRecord& one) { return one.structure == structure; });
}

bool ChunkStructures::add(StructureStartRecord start) {
    const bool known = std::ranges::any_of(starts_, [&start](const StructureStartRecord& one) {
        return one.structure == start.structure && one.chunkX == start.chunkX && one.chunkZ == start.chunkZ;
    });
    if (known) {
        return false;
    }
    starts_.push_back(std::move(start));
    return true;
}

void ChunkStructures::restore(std::vector<StructureStartRecord> starts) { starts_ = std::move(starts); }

} // namespace basalt::server::world::structure
