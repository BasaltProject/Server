#pragma once

#include <basalt/server/world/terrain/NetherFortressBrush.hpp>

namespace basalt::server::world::terrain::netherFortressPieces {

/// Writes whatever of `piece` lands inside `clip`. parity: `NetherFortressPieces` per-kind postProcess.
void place(const Piece& piece, Chunk& chunk, const structure::StructureBoundingBox& clip,
           random::RandomSource& placementRandom);

void placeBridgeCrossing(const Brush& brush);
void placeBridgeEnd(const Brush& brush);
void placeBridgeStraight(const Brush& brush);
void placeRoomCrossing(const Brush& brush);
void placeStairsRoom(const Brush& brush);
void placeMonsterThrone(const Brush& brush);

void placeCastleEntrance(const Brush& brush);
void placeCastleSmallCorridor(const Brush& brush);
void placeCastleSmallCrossing(const Brush& brush);
void placeCastleLeftTurn(const Brush& brush, random::RandomSource& random);
void placeCastleRightTurn(const Brush& brush, random::RandomSource& random);
void placeCastleStairs(const Brush& brush);
void placeCastleTBalcony(const Brush& brush);
void placeCastleStalkRoom(const Brush& brush);

} // namespace basalt::server::world::terrain::netherFortressPieces
