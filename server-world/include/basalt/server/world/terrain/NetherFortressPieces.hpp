#pragma once

#include <basalt/server/random/RandomSource.hpp>
#include <basalt/server/world/Chunk.hpp>
#include <basalt/server/world/structure/StructureTransform.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace basalt::server::world::terrain::netherFortressPieces {

enum class PieceType : std::uint8_t {
    Start,
    BridgeCrossing,
    BridgeEndFiller,
    BridgeStraight,
    RoomCrossing,
    StairsRoom,
    MonsterThrone,
    CastleEntrance,
    CastleSmallCorridor,
    CastleSmallCrossing,
    CastleLeftTurn,
    CastleRightTurn,
    CastleStairs,
    CastleTBalcony,
    CastleStalkRoom,
};

struct Piece {
    PieceType type = PieceType::Start;
    std::int32_t generationDepth = 0;
    structure::StructureBoundingBox boundingBox;
    structure::StructureDirection orientation = structure::StructureDirection::North;
    std::optional<std::int32_t> selfSeed;
    bool needsChest = false;
};

/// parity: `NetherFortressStructure.generatePieces` and `NetherFortressPieces` layout assembly.
[[nodiscard]] std::vector<Piece> assemble(random::RandomSource& random, std::int32_t chunkX, std::int32_t chunkZ);

/// parity: `StructurePiece.isReplaceableByStructures`.
[[nodiscard]] bool replaceableByStructures(std::int32_t stateId);

[[nodiscard]] std::int32_t air();
[[nodiscard]] std::int32_t netherBricks();
[[nodiscard]] std::int32_t netherBrickFence();
[[nodiscard]] std::int32_t soulSand();
[[nodiscard]] std::int32_t netherWart();
[[nodiscard]] std::int32_t lava();
[[nodiscard]] std::int32_t spawner();

} // namespace basalt::server::world::terrain::netherFortressPieces
