#include <basalt/server/world/structure/StructureQueries.hpp>

namespace basalt::server::world::structure {

std::span<const StructureStartRecord> StructureQueries::startsIn(const StructurePos pos) const {
    const ChunkStructures* chunk = chunkStructures_(pos.x >> 4, pos.z >> 4);
    return chunk == nullptr ? std::span<const StructureStartRecord>{} : chunk->starts();
}

const StructureStartRecord* StructureQueries::startAt(const StructurePos pos, const std::string_view structure) const {
    for (const StructureStartRecord& start : startsIn(pos)) {
        if (start.structure == structure && start.contains(pos)) {
            return &start;
        }
    }
    return nullptr;
}

const StructureStartRecord* StructureQueries::startWithPieceAt(const StructurePos pos,
                                                               const std::string_view structure) const {
    for (const StructureStartRecord& start : startsIn(pos)) {
        if (start.structure == structure && start.hasPieceAt(pos)) {
            return &start;
        }
    }
    return nullptr;
}

} // namespace basalt::server::world::structure
