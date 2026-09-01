#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `MineshaftStructure.Type`. Decides the wood a mineshaft is framed in.
enum class MineshaftType : std::uint8_t { Normal, Mesa };

/// One assembled mineshaft piece: what it is, where it is, and the rolls its blocks will need.
struct MineshaftPiece {
    enum class Kind : std::uint8_t { Corridor, Crossing, Stairs, Room };

    Kind kind = Kind::Corridor;
    StructureBoundingBox box;
    /// The direction the piece was entered from. Absent on the starting room, which has none.
    std::optional<StructureDirection> orientation;
    std::int32_t genDepth = 0;
    bool hasRails = false;
    bool spiderCorridor = false;
    std::int32_t sections = 0;
    bool twoFloored = false;
    /// Only the room has these: the mouth it cut for each passage leaving it.
    std::vector<StructureBoundingBox> entrances;

    /// The orientation the blocks are written with, not always the one the piece was entered from.
    /// parity: only the corridor and the stairs call `setOrientation`.
    [[nodiscard]] std::optional<StructureDirection> fillOrientation() const {
        return kind == Kind::Corridor || kind == Kind::Stairs ? orientation : std::nullopt;
    }
};

/// parity: `MineshaftPieces` plus `MineshaftStructure.generatePiecesAndAdjust`: which pieces exist,
/// where each sits and which way it faces. Split from the block filling so a layout can be compared
/// against a live world's `structures.starts` before a plank is placed.
namespace mineshaftLayout {

/// The pieces of the mineshaft starting at `chunkX`/`chunkZ`, in the order vanilla adds them.
[[nodiscard]] std::vector<MineshaftPiece> assemble(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ,
                                                   MineshaftType type, std::int32_t seaLevel, std::int32_t minY);

} // namespace mineshaftLayout

} // namespace basalt::server::world::structure
