#pragma once

#include <basalt/server/world/structure/StrongholdLayout.hpp>

namespace basalt::server::world::structure {

/// The pieces that only carry the maze along. parity: `StrongholdPieces`' Straight.
namespace strongholdCorridors {

/// parity: `StairsDown`, which is also the start piece: a spiral of brick and slab.
void stairsDown(const StrongholdBrush& brush, const StrongholdPiece& piece);
/// parity: `StraightStairsDown`, the long flight of cobblestone stairs.
void straightStairsDown(const StrongholdBrush& brush, const StrongholdPiece& piece);
/// parity: `Straight`. The four torch draws happen whether or not a torch lands.
void straight(const StrongholdBrush& brush, const StrongholdPiece& piece);
/// parity: `LeftTurn` and `RightTurn`.
void turn(const StrongholdBrush& brush, const StrongholdPiece& piece);
/// parity: `ChestCorridor`.
void chestCorridor(const StrongholdBrush& brush, const StrongholdPiece& piece);
/// parity: `FillerCorridor`.
void fillerCorridor(const StrongholdBrush& brush, const StrongholdPiece& piece);

} // namespace strongholdCorridors

/// The three wide rooms. parity: `StrongholdPieces`' PrisonHall, RoomCrossing and FiveCrossing.
namespace strongholdRooms {

/// parity: `PrisonHall`, whose four cells are iron bars and iron doors.
void prisonHall(const StrongholdBrush& brush, const StrongholdPiece& piece);
/// parity: `RoomCrossing`. Its five interiors; the last two are simply an empty room.
void roomCrossing(const StrongholdBrush& brush, const StrongholdPiece& piece);
/// parity: `FiveCrossing`, the two-storey junction with up to four side openings.
void fiveCrossing(const StrongholdBrush& brush, const StrongholdPiece& piece);

} // namespace strongholdRooms

} // namespace basalt::server::world::structure
