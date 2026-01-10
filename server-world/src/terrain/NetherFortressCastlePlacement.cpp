#include <basalt/server/world/terrain/NetherFortressPlacement.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>

namespace basalt::server::world::terrain::netherFortressPieces {
namespace {

using structure::StructureDirection;

void placeCastleOuterShell(const Brush& brush, bool frontGate);
void placeCastleFoundation(const Brush& brush);

void placeCastleTurn(const Brush& brush, bool wallOnLeft) {
    brush.box(0, 0, 0, 4, 1, 4, netherBricks());
    brush.box(0, 2, 0, 4, 5, 4, air());
    const std::int32_t wallX = wallOnLeft ? 0 : 4;
    brush.box(wallX, 2, 0, wallX, 5, 4, netherBricks());
    const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
    const std::int32_t weFence = brush.fence({StructureDirection::West, StructureDirection::East});
    for (const std::int32_t z : {1, 3}) {
        brush.box(wallX, 3, z, wallX, 4, z, nsFence);
    }
    if (wallOnLeft) {
        brush.box(4, 2, 0, 4, 5, 0, netherBricks());
        brush.box(1, 2, 4, 4, 5, 4, netherBricks());
    } else {
        brush.box(0, 2, 0, 0, 5, 0, netherBricks());
        brush.box(0, 2, 4, 3, 5, 4, netherBricks());
    }
    for (const std::int32_t x : {1, 3}) {
        brush.box(x, 3, 4, x, 4, 4, weFence);
    }
    brush.box(0, 6, 0, 4, 6, 4, netherBricks());
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t z = 0; z <= 4; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
        }
    }
}

void placeFortressChest(const Brush& brush, std::int32_t x, std::int32_t y, std::int32_t z,
                        random::RandomSource& random) {
    const Piece& piece = brush.piece();
    const bool facesSouth =
        (piece.type == PieceType::CastleLeftTurn && piece.orientation == StructureDirection::North) ||
        (piece.type == PieceType::CastleRightTurn && piece.orientation != StructureDirection::South);
    const StructureDirection facing = facesSouth ? StructureDirection::South : StructureDirection::North;
    const std::int32_t stateId = stateOf(
        "chest",
        {{"facing", std::string{structure::serializedName(facing)}}, {"type", "single"}, {"waterlogged", "false"}});
    const std::int32_t worldX = brush.worldX(x, z);
    const std::int32_t worldY = piece.boundingBox.minY + y;
    const std::int32_t worldZ = brush.worldZ(x, z);
    brush.chunk().setBlockStateId(worldX & 15, worldY, worldZ & 15, stateId);
    wire::nbt::NbtCompound data;
    data.set("id", wire::nbt::NbtTag::ofString("minecraft:chest"));
    data.set("LootTable", wire::nbt::NbtTag::ofString("minecraft:chests/nether_bridge"));
    data.set("LootTableSeed", wire::nbt::NbtTag::ofLong(random.nextLong()));
    data.set("components", wire::nbt::NbtTag::ofCompound(wire::nbt::NbtCompound{}));
    brush.chunk().setBlockEntity(ChunkBlockEntity{worldX & 15, worldY, worldZ & 15,
                                                  wire::common::Identifier::vanilla("chest"), std::move(data)});
}

void placeCastleOuterShell(const Brush& brush, bool frontGate) {
    brush.box(0, 3, 0, 12, 4, 12, netherBricks());
    brush.box(0, 5, 0, 12, 13, 12, air());
    brush.box(0, 5, 0, 1, 12, 12, netherBricks());
    brush.box(11, 5, 0, 12, 12, 12, netherBricks());
    brush.box(2, 5, 11, 4, 12, 12, netherBricks());
    brush.box(8, 5, 11, 10, 12, 12, netherBricks());
    brush.box(5, 9, 11, 7, 12, 12, netherBricks());
    brush.box(2, 5, 0, 4, 12, 1, netherBricks());
    brush.box(8, 5, 0, 10, 12, 1, netherBricks());
    brush.box(5, 9, 0, 7, 12, 1, netherBricks());
    brush.box(2, 11, 2, 10, 12, 10, netherBricks());
    if (frontGate) {
        brush.box(5, 8, 0, 7, 8, 0, netherBrickFence());
    }
    const std::int32_t weFence = brush.fence({StructureDirection::West, StructureDirection::East});
    const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
    for (std::int32_t i = 1; i <= 11; i += 2) {
        brush.box(i, 10, 0, i, 11, 0, weFence);
        brush.box(i, 10, 12, i, 11, 12, weFence);
        brush.box(0, 10, i, 0, 11, i, nsFence);
        brush.box(12, 10, i, 12, 11, i, nsFence);
        brush.placeBlock(i, 13, 0, netherBricks());
        brush.placeBlock(i, 13, 12, netherBricks());
        brush.placeBlock(0, 13, i, netherBricks());
        brush.placeBlock(12, 13, i, netherBricks());
        if (i != 11) {
            brush.placeBlock(i + 1, 13, 0, weFence);
            brush.placeBlock(i + 1, 13, 12, weFence);
            brush.placeBlock(0, 13, i + 1, nsFence);
            brush.placeBlock(12, 13, i + 1, nsFence);
        }
    }
    brush.placeBlock(0, 13, 0, brush.fence({StructureDirection::North, StructureDirection::East}));
    brush.placeBlock(0, 13, 12, brush.fence({StructureDirection::South, StructureDirection::East}));
    brush.placeBlock(12, 13, 12, brush.fence({StructureDirection::South, StructureDirection::West}));
    brush.placeBlock(12, 13, 0, brush.fence({StructureDirection::North, StructureDirection::West}));
    for (std::int32_t z = 3; z <= 9; z += 2) {
        brush.box(1, 7, z, 1, 8, z,
                  brush.fence({StructureDirection::North, StructureDirection::South, StructureDirection::West}));
        brush.box(11, 7, z, 11, 8, z,
                  brush.fence({StructureDirection::North, StructureDirection::South, StructureDirection::East}));
    }
}

void placeCastleFoundation(const Brush& brush) {
    brush.box(4, 2, 0, 8, 2, 12, netherBricks());
    brush.box(0, 2, 4, 12, 2, 8, netherBricks());
    brush.box(4, 0, 0, 8, 1, 3, netherBricks());
    brush.box(4, 0, 9, 8, 1, 12, netherBricks());
    brush.box(0, 0, 4, 3, 1, 8, netherBricks());
    brush.box(9, 0, 4, 12, 1, 8, netherBricks());
    for (std::int32_t x = 4; x <= 8; ++x) {
        for (std::int32_t z = 0; z <= 2; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
            brush.fillDown(x, -1, 12 - z, netherBricks());
        }
    }
    for (std::int32_t x = 0; x <= 2; ++x) {
        for (std::int32_t z = 4; z <= 8; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
            brush.fillDown(12 - x, -1, z, netherBricks());
        }
    }
}

} // namespace

void placeCastleEntrance(const Brush& brush) {
    placeCastleOuterShell(brush, true);
    placeCastleFoundation(brush);
    brush.box(5, 5, 5, 7, 5, 7, netherBricks());
    brush.box(6, 1, 6, 6, 4, 6, air());
    brush.placeBlock(6, 0, 6, netherBricks());
    brush.placeBlock(6, 5, 6, lava());
    if (brush.isInside(6, 5, 6)) {
        brush.chunk().scheduleFluidTick(ChunkScheduledTick{.type = wire::common::Identifier::vanilla("lava"),
                                                           .localX = brush.worldX(6, 6) & 15,
                                                           .y = brush.piece().boundingBox.minY + 5,
                                                           .localZ = brush.worldZ(6, 6) & 15,
                                                           .delay = 0});
    }
}

void placeCastleSmallCorridor(const Brush& brush) {
    brush.box(0, 0, 0, 4, 1, 4, netherBricks());
    brush.box(0, 2, 0, 4, 5, 4, air());
    brush.box(0, 2, 0, 0, 5, 4, netherBricks());
    brush.box(4, 2, 0, 4, 5, 4, netherBricks());
    const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
    for (const std::int32_t x : {0, 4}) {
        for (const std::int32_t z : {1, 3}) {
            brush.box(x, 3, z, x, 4, z, nsFence);
        }
    }
    brush.box(0, 6, 0, 4, 6, 4, netherBricks());
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t z = 0; z <= 4; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
        }
    }
}

void placeCastleSmallCrossing(const Brush& brush) {
    brush.box(0, 0, 0, 4, 1, 4, netherBricks());
    brush.box(0, 2, 0, 4, 5, 4, air());
    brush.box(0, 2, 0, 0, 5, 0, netherBricks());
    brush.box(4, 2, 0, 4, 5, 0, netherBricks());
    brush.box(0, 2, 4, 0, 5, 4, netherBricks());
    brush.box(4, 2, 4, 4, 5, 4, netherBricks());
    brush.box(0, 6, 0, 4, 6, 4, netherBricks());
    for (std::int32_t x = 0; x <= 4; ++x) {
        for (std::int32_t z = 0; z <= 4; ++z) {
            brush.fillDown(x, -1, z, netherBricks());
        }
    }
}

void placeCastleLeftTurn(const Brush& brush, random::RandomSource& random) {
    placeCastleTurn(brush, false);
    if (brush.piece().needsChest && brush.isInside(3, 2, 3) && !brush.isChestAt(3, 2, 3)) {
        placeFortressChest(brush, 3, 2, 3, random);
    }
}

void placeCastleRightTurn(const Brush& brush, random::RandomSource& random) {
    placeCastleTurn(brush, true);
    if (brush.piece().needsChest && brush.isInside(1, 2, 3) && !brush.isChestAt(1, 2, 3)) {
        placeFortressChest(brush, 1, 2, 3, random);
    }
}

void placeCastleStairs(const Brush& brush) {
    for (std::int32_t step = 0; step <= 9; ++step) {
        const std::int32_t floor = std::max(1, 7 - step);
        const std::int32_t roof = std::min(std::max(floor + 5, 14 - step), 13);
        brush.box(0, 0, step, 4, floor, step, netherBricks());
        brush.box(1, floor + 1, step, 3, roof - 1, step, air());
        if (step <= 6) {
            brush.box(1, floor + 1, step, 3, floor + 1, step, brush.stairs(StructureDirection::South));
        }
        brush.box(0, roof, step, 4, roof, step, netherBricks());
        brush.box(0, floor + 1, step, 0, roof - 1, step, netherBricks());
        brush.box(4, floor + 1, step, 4, roof - 1, step, netherBricks());
        if ((step & 1) == 0) {
            const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
            brush.box(0, floor + 2, step, 0, floor + 3, step, nsFence);
            brush.box(4, floor + 2, step, 4, floor + 3, step, nsFence);
        }
        for (std::int32_t x = 0; x <= 4; ++x) {
            brush.fillDown(x, -1, step, netherBricks());
        }
    }
}

void placeCastleTBalcony(const Brush& brush) {
    const std::int32_t nsFence = brush.fence({StructureDirection::North, StructureDirection::South});
    const std::int32_t weFence = brush.fence({StructureDirection::West, StructureDirection::East});
    brush.box(0, 0, 0, 8, 1, 8, netherBricks());
    brush.box(0, 2, 0, 8, 5, 8, air());
    brush.box(0, 6, 0, 8, 6, 5, netherBricks());
    brush.box(0, 2, 0, 2, 5, 0, netherBricks());
    brush.box(6, 2, 0, 8, 5, 0, netherBricks());
    brush.box(1, 3, 0, 1, 4, 0, weFence);
    brush.box(7, 3, 0, 7, 4, 0, weFence);
    brush.box(0, 2, 4, 8, 2, 8, netherBricks());
    brush.box(1, 1, 4, 2, 2, 4, air());
    brush.box(6, 1, 4, 7, 2, 4, air());
    brush.box(1, 3, 8, 7, 3, 8, weFence);
    brush.placeBlock(0, 3, 8, brush.fence({StructureDirection::East, StructureDirection::South}));
    brush.placeBlock(8, 3, 8, brush.fence({StructureDirection::West, StructureDirection::South}));
    brush.box(0, 3, 6, 0, 3, 7, nsFence);
    brush.box(8, 3, 6, 8, 3, 7, nsFence);
    brush.box(0, 3, 4, 0, 5, 5, netherBricks());
    brush.box(8, 3, 4, 8, 5, 5, netherBricks());
    brush.box(1, 3, 5, 2, 5, 5, netherBricks());
    brush.box(6, 3, 5, 7, 5, 5, netherBricks());
    brush.box(1, 4, 5, 1, 5, 5, weFence);
    brush.box(7, 4, 5, 7, 5, 5, weFence);
    for (std::int32_t z = 0; z <= 5; ++z) {
        for (std::int32_t x = 0; x <= 8; ++x) {
            brush.fillDown(x, -1, z, netherBricks());
        }
    }
}

void placeCastleStalkRoom(const Brush& brush) {
    placeCastleOuterShell(brush, false);
    const std::int32_t northStairs = brush.stairs(StructureDirection::North);
    const std::int32_t nseFence =
        brush.fence({StructureDirection::North, StructureDirection::South, StructureDirection::East});
    const std::int32_t nswFence =
        brush.fence({StructureDirection::North, StructureDirection::South, StructureDirection::West});
    for (std::int32_t i = 0; i <= 6; ++i) {
        const std::int32_t z = i + 4;
        brush.box(5, 5 + i, z, 7, 5 + i, z, northStairs);
        if (z >= 5 && z <= 8) {
            brush.box(5, 5, z, 7, i + 4, z, netherBricks());
        } else if (z >= 9 && z <= 10) {
            brush.box(5, 8, z, 7, i + 4, z, netherBricks());
        }
        if (i >= 1) {
            brush.box(5, 6 + i, z, 7, 9 + i, z, air());
        }
    }
    brush.box(5, 12, 11, 7, 12, 11, northStairs);
    brush.box(5, 6, 7, 5, 7, 7, nseFence);
    brush.box(7, 6, 7, 7, 7, 7, nswFence);
    brush.box(5, 13, 12, 7, 13, 12, air());
    brush.box(2, 5, 2, 3, 5, 3, netherBricks());
    brush.box(2, 5, 9, 3, 5, 10, netherBricks());
    brush.box(2, 5, 4, 2, 5, 8, netherBricks());
    brush.box(9, 5, 2, 10, 5, 3, netherBricks());
    brush.box(9, 5, 9, 10, 5, 10, netherBricks());
    brush.box(10, 5, 4, 10, 5, 8, netherBricks());
    const std::int32_t westStairs = brush.stairs(StructureDirection::West);
    const std::int32_t eastStairs = brush.stairs(StructureDirection::East);
    for (const std::int32_t z : {2, 3, 9, 10}) {
        brush.placeBlock(4, 5, z, westStairs);
        brush.placeBlock(8, 5, z, eastStairs);
    }
    brush.box(3, 4, 4, 4, 4, 8, soulSand());
    brush.box(8, 4, 4, 9, 4, 8, soulSand());
    brush.box(3, 5, 4, 4, 5, 8, netherWart());
    brush.box(8, 5, 4, 9, 5, 8, netherWart());
    placeCastleFoundation(brush);
}

void place(const Piece& piece, Chunk& chunk, const structure::StructureBoundingBox& clip,
           random::RandomSource& placementRandom) {
    const Brush brush{piece, chunk, clip};
    switch (piece.type) {
    case PieceType::Start:
    case PieceType::BridgeCrossing:
        placeBridgeCrossing(brush);
        break;
    case PieceType::BridgeStraight:
        placeBridgeStraight(brush);
        break;
    case PieceType::BridgeEndFiller:
        placeBridgeEnd(brush);
        break;
    case PieceType::RoomCrossing:
        placeRoomCrossing(brush);
        break;
    case PieceType::StairsRoom:
        placeStairsRoom(brush);
        break;
    case PieceType::MonsterThrone:
        placeMonsterThrone(brush);
        break;
    case PieceType::CastleEntrance:
        placeCastleEntrance(brush);
        break;
    case PieceType::CastleSmallCorridor:
        placeCastleSmallCorridor(brush);
        break;
    case PieceType::CastleSmallCrossing:
        placeCastleSmallCrossing(brush);
        break;
    case PieceType::CastleLeftTurn:
        placeCastleLeftTurn(brush, placementRandom);
        break;
    case PieceType::CastleRightTurn:
        placeCastleRightTurn(brush, placementRandom);
        break;
    case PieceType::CastleStairs:
        placeCastleStairs(brush);
        break;
    case PieceType::CastleTBalcony:
        placeCastleTBalcony(brush);
        break;
    case PieceType::CastleStalkRoom:
    default:
        placeCastleStalkRoom(brush);
        break;
    }
}

} // namespace basalt::server::world::terrain::netherFortressPieces
