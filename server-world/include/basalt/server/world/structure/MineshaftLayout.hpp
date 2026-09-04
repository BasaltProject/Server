#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/MineshaftBlocks.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::world::structure {

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

/// An assembled mineshaft: its pieces and the position vanilla reports as the structure's start.
struct Assembly {
    std::vector<MineshaftPiece> pieces;
    /// parity: `findGenerationPoint` starts at y 50 and offsets by whatever the vertical move applied,
    /// and that is the y whose biome decides whether the mineshaft is kept.
    std::int32_t stubY = 0;
};

/// The `WORLD_SURFACE_WG` height of a column, which only the mesa variant needs.
using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

/// The pieces of the mineshaft starting at `chunkX`/`chunkZ`, in the order vanilla adds them.
[[nodiscard]] Assembly assemble(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, MineshaftType type,
                                std::int32_t seaLevel, std::int32_t minY, const SurfaceHeightAt& surfaceHeightAt = {});

} // namespace mineshaftLayout

} // namespace basalt::server::world::structure
