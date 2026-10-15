#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/structure/StrongholdBrush.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <functional>
#include <vector>

namespace basalt::server::world::structure {

/// parity: `StrongholdPieces`' eleven piece classes, plus the filler and the start.
enum class StrongholdPieceKind : std::uint8_t {
    Start,
    Straight,
    PrisonHall,
    LeftTurn,
    RightTurn,
    RoomCrossing,
    StraightStairsDown,
    StairsDown,
    FiveCrossing,
    ChestCorridor,
    Library,
    PortalRoom,
    FillerCorridor,
};

struct StrongholdPiece {
    StrongholdPieceKind kind = StrongholdPieceKind::Start;
    StructureBoundingBox box;
    StructureDirection orientation = StructureDirection::North;
    std::int32_t depth = 0;
    StrongholdDoor entryDoor = StrongholdDoor::Opening;
    bool leftLow = false;
    bool leftHigh = false;
    bool rightLow = false;
    bool rightHigh = false;
    bool leftChild = false;
    bool rightChild = false;
    std::int32_t roomType = 0;
    bool tall = false;
    bool isSource = false;
    std::int32_t steps = 0;
};

/// parity: `StrongholdStructure.generatePieces`.
namespace strongholdLayout {

using Reseed = std::function<void(std::int32_t)>;

[[nodiscard]] std::vector<StrongholdPiece> build(random::RandomSource& random, const Reseed& reseed,
                                                 std::int32_t chunkX, std::int32_t chunkZ, std::int32_t seaLevel,
                                                 std::int32_t minY);

/// parity: `StructurePiece.makeBoundingBox`, which swaps width and depth on the X axis.
[[nodiscard]] StructureBoundingBox makeBox(std::int32_t x, std::int32_t y, std::int32_t z, StructureDirection direction,
                                           std::int32_t width, std::int32_t height, std::int32_t depth);

/// parity: `BoundingBox.orientBox`, which grows the box away from the door it hangs off.
[[nodiscard]] StructureBoundingBox orientBox(std::int32_t footX, std::int32_t footY, std::int32_t footZ,
                                             std::int32_t offX, std::int32_t offY, std::int32_t offZ,
                                             std::int32_t width, std::int32_t height, std::int32_t depth,
                                             StructureDirection direction);

} // namespace strongholdLayout

} // namespace basalt::server::world::structure
