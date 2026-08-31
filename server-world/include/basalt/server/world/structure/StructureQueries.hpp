#pragma once

#include <basalt/server/world/structure/StructureBounds.hpp>

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

namespace basalt::server::world::structure {

/// parity: `StructureManager`, the position queries only. The chunk lookup keeps this safe under
/// regional ownership: no query touches a chunk other than the one holding the position.
class StructureQueries {
public:
    using ChunkLookup = std::function<const ChunkStructures*(std::int32_t, std::int32_t)>;

    explicit StructureQueries(ChunkLookup chunkStructures) : chunkStructures_(std::move(chunkStructures)) {}

    /// parity: `getStructureAt`, matching the start's full bounds.
    [[nodiscard]] const StructureStartRecord* startAt(StructurePos pos, std::string_view structure) const;

    /// parity: `getStructureWithPieceAt`, matching a piece's own bounds.
    [[nodiscard]] const StructureStartRecord* startWithPieceAt(StructurePos pos, std::string_view structure) const;

    /// Every start reaching the position's chunk, in the order that chunk recorded them, which is
    /// the order a query resolves ties in when a position belongs to two structures at once.
    [[nodiscard]] std::span<const StructureStartRecord> startsIn(StructurePos pos) const;

private:
    ChunkLookup chunkStructures_;
};

} // namespace basalt::server::world::structure
