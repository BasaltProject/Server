#include <basalt/server/world/structure/StrongholdRooms.hpp>

#include <cstdint>

namespace basalt::server::world::structure {

void strongholdCorridors::stairsDown(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    const std::int32_t bricks = strongholdPalette::stoneBricks();
    const std::int32_t slab = strongholdPalette::smoothStoneSlab();
    brush.shell(0, 0, 0, 4, 10, 4);
    brush.door(piece.entryDoor, 1, 7, 0);
    brush.door(StrongholdDoor::Opening, 1, 1, 4);
    brush.place(bricks, 2, 6, 1);
    brush.place(bricks, 1, 5, 1);
    brush.place(slab, 1, 6, 1);
    brush.place(bricks, 1, 5, 2);
    brush.place(bricks, 1, 4, 3);
    brush.place(slab, 1, 5, 3);
    brush.place(bricks, 2, 4, 3);
    brush.place(bricks, 3, 3, 3);
    brush.place(slab, 3, 4, 3);
    brush.place(bricks, 3, 3, 2);
    brush.place(bricks, 3, 2, 1);
    brush.place(slab, 3, 3, 1);
    brush.place(bricks, 2, 2, 1);
    brush.place(bricks, 1, 1, 1);
    brush.place(slab, 1, 2, 1);
    brush.place(bricks, 1, 1, 2);
    brush.place(slab, 1, 1, 3);
}

void strongholdCorridors::straightStairsDown(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    brush.shell(0, 0, 0, 4, 10, 7);
    brush.door(piece.entryDoor, 1, 7, 0);
    brush.door(StrongholdDoor::Opening, 1, 1, 7);
    const std::int32_t stairs = strongholdPalette::stairs("cobblestone_stairs", "south");
    const std::int32_t bricks = strongholdPalette::stoneBricks();
    for (std::int32_t step = 0; step < 6; ++step) {
        brush.place(stairs, 1, 6 - step, 1 + step);
        brush.place(stairs, 2, 6 - step, 1 + step);
        brush.place(stairs, 3, 6 - step, 1 + step);
        if (step < 5) {
            brush.place(bricks, 1, 5 - step, 1 + step);
            brush.place(bricks, 2, 5 - step, 1 + step);
            brush.place(bricks, 3, 5 - step, 1 + step);
        }
    }
}

void strongholdCorridors::straight(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    brush.shell(0, 0, 0, 4, 4, 6);
    brush.door(piece.entryDoor, 1, 1, 0);
    brush.door(StrongholdDoor::Opening, 1, 1, 6);
    const std::int32_t east = strongholdPalette::wallTorch("east");
    const std::int32_t west = strongholdPalette::wallTorch("west");
    brush.maybe(0.1F, 1, 2, 1, east);
    brush.maybe(0.1F, 3, 2, 1, west);
    brush.maybe(0.1F, 1, 2, 5, east);
    brush.maybe(0.1F, 3, 2, 5, west);
    if (piece.leftChild) {
        brush.box(0, 1, 2, 0, 3, 4, strongholdPalette::caveAir());
    }
    if (piece.rightChild) {
        brush.box(4, 1, 2, 4, 3, 4, strongholdPalette::caveAir());
    }
}

void strongholdCorridors::turn(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    brush.shell(0, 0, 0, 4, 4, 4);
    brush.door(piece.entryDoor, 1, 1, 0);
    const bool flipped =
        piece.orientation != StructureDirection::North && piece.orientation != StructureDirection::East;
    const bool left = piece.kind == StrongholdPieceKind::LeftTurn;
    const bool onEast = left ? flipped : !flipped;
    if (onEast) {
        brush.box(4, 1, 1, 4, 3, 3, strongholdPalette::caveAir());
    } else {
        brush.box(0, 1, 1, 0, 3, 3, strongholdPalette::caveAir());
    }
}

void strongholdCorridors::chestCorridor(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    const std::int32_t slab = strongholdPalette::stoneBrickSlab();
    brush.shell(0, 0, 0, 4, 4, 6);
    brush.door(piece.entryDoor, 1, 1, 0);
    brush.door(StrongholdDoor::Opening, 1, 1, 6);
    brush.box(3, 1, 2, 3, 1, 4, strongholdPalette::stoneBricks());
    brush.place(slab, 3, 1, 1);
    brush.place(slab, 3, 1, 5);
    brush.place(slab, 3, 2, 2);
    brush.place(slab, 3, 2, 4);
    for (std::int32_t z = 2; z <= 4; ++z) {
        brush.place(slab, 2, 1, z);
    }
    brush.createChest(3, 2, 3, "minecraft:chests/stronghold_corridor");
}

void strongholdCorridors::fillerCorridor(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    const std::int32_t bricks = strongholdPalette::stoneBricks();
    const std::int32_t caveAir = strongholdPalette::caveAir();
    for (std::int32_t z = 0; z < piece.steps; ++z) {
        for (std::int32_t x = 0; x <= 4; ++x) {
            brush.place(bricks, x, 0, z);
        }
        for (std::int32_t y = 1; y <= 3; ++y) {
            brush.place(bricks, 0, y, z);
            brush.place(caveAir, 1, y, z);
            brush.place(caveAir, 2, y, z);
            brush.place(caveAir, 3, y, z);
            brush.place(bricks, 4, y, z);
        }
        for (std::int32_t x = 0; x <= 4; ++x) {
            brush.place(bricks, x, 4, z);
        }
    }
}

} // namespace basalt::server::world::structure
