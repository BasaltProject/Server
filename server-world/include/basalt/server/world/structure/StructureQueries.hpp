#pragma once

#include <basalt/server/world/structure/StructureBounds.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace basalt::server::world::structure {

/// parity: `StructureManager`.
class StructureQueries {
public:
    using ChunkLookup = std::function<const ChunkStructures*(std::int32_t, std::int32_t)>;

    explicit StructureQueries(ChunkLookup chunkStructures) : chunkStructures_(std::move(chunkStructures)) {}

    /// parity: `getStructureAt`, matching the start's full bounds.
    [[nodiscard]] const StructureStartRecord* startAt(StructurePos pos, std::string_view structure) const;

    /// parity: `getStructureWithPieceAt`, matching a piece's own bounds.
    [[nodiscard]] const StructureStartRecord* startWithPieceAt(StructurePos pos, std::string_view structure) const;

    [[nodiscard]] std::span<const StructureStartRecord> startsIn(StructurePos pos) const;

private:
    ChunkLookup chunkStructures_;
};

} // namespace basalt::server::world::structure
