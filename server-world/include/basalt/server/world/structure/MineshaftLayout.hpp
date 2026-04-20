#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/MineshaftBlocks.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace basalt::server::world::structure {

struct MineshaftPiece {
    enum class Kind : std::uint8_t { Corridor, Crossing, Stairs, Room };

    Kind kind = Kind::Corridor;
    StructureBoundingBox box;
    std::optional<StructureDirection> orientation;
    std::int32_t genDepth = 0;
    bool hasRails = false;
    bool spiderCorridor = false;
    std::int32_t sections = 0;
    bool twoFloored = false;
    std::vector<StructureBoundingBox> entrances;

    /// parity: only the corridor and the stairs call `setOrientation`.
    [[nodiscard]] std::optional<StructureDirection> fillOrientation() const {
        return kind == Kind::Corridor || kind == Kind::Stairs ? orientation : std::nullopt;
    }
};

/// parity: `MineshaftPieces` plus `MineshaftStructure.generatePiecesAndAdjust`.
namespace mineshaftLayout {

struct Assembly {
    std::vector<MineshaftPiece> pieces;
    /// parity: `findGenerationPoint` starts at y 50 and offsets by whatever the vertical move applied.
    std::int32_t stubY = 0;
};

using SurfaceHeightAt = std::function<std::int32_t(std::int32_t, std::int32_t)>;

[[nodiscard]] Assembly assemble(std::int64_t seed, std::int32_t chunkX, std::int32_t chunkZ, MineshaftType type,
                                std::int32_t seaLevel, std::int32_t minY, const SurfaceHeightAt& surfaceHeightAt = {});

} // namespace mineshaftLayout

} // namespace basalt::server::world::structure
