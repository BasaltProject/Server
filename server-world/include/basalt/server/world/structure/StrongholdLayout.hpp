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

/// One room of the maze. The per-kind fields are the draws its constructor made, kept here because
/// the geometry needs them later and redrawing would move the shared random.
struct StrongholdPiece {
    StrongholdPieceKind kind = StrongholdPieceKind::Start;
    StructureBoundingBox box;
    StructureDirection orientation = StructureDirection::North;
    std::int32_t depth = 0;
    StrongholdDoor entryDoor = StrongholdDoor::Opening;
    /// FiveCrossing's four optional doorways.
    bool leftLow = false;
    bool leftHigh = false;
    bool rightLow = false;
    bool rightHigh = false;
    /// Straight's two optional side doorways.
    bool leftChild = false;
    bool rightChild = false;
    /// RoomCrossing's interior, one of five.
    std::int32_t roomType = 0;
    /// Library is two storeys when it had room for eleven.
    bool tall = false;
    /// Only the start piece, which is the one that forces a five-way next.
    bool isSource = false;
    /// FillerCorridor's length along its own axis.
    std::int32_t steps = 0;
};

/// parity: `StrongholdStructure.generatePieces`. The maze is grown breadth-first but in random order:
/// the pending list is drained by drawing an index, so the shape depends on the whole history. It is
/// retried, seed bumped by one each time, until a portal room appears.
namespace strongholdLayout {

/// Reseeds `random` for attempt `n`, which is how vanilla retries a maze that grew no portal room.
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
