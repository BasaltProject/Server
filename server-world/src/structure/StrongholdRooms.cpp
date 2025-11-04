#include <basalt/server/world/structure/StrongholdRooms.hpp>

namespace basalt::server::world::structure {
namespace {

[[nodiscard]] std::int32_t oakPlanks() {
    static const std::int32_t stateId = strongholdPalette::state("oak_planks");
    return stateId;
}

[[nodiscard]] std::int32_t water() {
    static const std::int32_t stateId = strongholdPalette::state("water");
    return stateId;
}

[[nodiscard]] std::int32_t doubleSlab() {
    static const std::int32_t stateId =
        strongholdPalette::state("smooth_stone_slab", {{"type", "double"}, {"waterlogged", "false"}});
    return stateId;
}

[[nodiscard]] std::int32_t ladderWest() {
    static const std::int32_t stateId =
        strongholdPalette::state("ladder", {{"facing", "west"}, {"waterlogged", "false"}});
    return stateId;
}

/// Interior 0: a lit pillar ringed with slabs.
void pillarRoom(const StrongholdBrush& brush) {
    const std::int32_t bricks = strongholdPalette::stoneBricks();
    const std::int32_t slab = strongholdPalette::smoothStoneSlab();
    brush.place(bricks, 5, 1, 5);
    brush.place(bricks, 5, 2, 5);
    brush.place(bricks, 5, 3, 5);
    brush.place(strongholdPalette::wallTorch("west"), 4, 3, 5);
    brush.place(strongholdPalette::wallTorch("east"), 6, 3, 5);
    brush.place(strongholdPalette::wallTorch("south"), 5, 3, 4);
    brush.place(strongholdPalette::wallTorch("north"), 5, 3, 6);
    brush.place(slab, 4, 1, 4);
    brush.place(slab, 4, 1, 5);
    brush.place(slab, 4, 1, 6);
    brush.place(slab, 6, 1, 4);
    brush.place(slab, 6, 1, 5);
    brush.place(slab, 6, 1, 6);
    brush.place(slab, 5, 1, 4);
    brush.place(slab, 5, 1, 6);
}

/// Interior 1: a brick ring around a pillar with water on top.
void fountainRoom(const StrongholdBrush& brush) {
    const std::int32_t bricks = strongholdPalette::stoneBricks();
    for (std::int32_t offset = 0; offset < 5; ++offset) {
        brush.place(bricks, 3, 1, 3 + offset);
        brush.place(bricks, 7, 1, 3 + offset);
        brush.place(bricks, 3 + offset, 1, 3);
        brush.place(bricks, 3 + offset, 1, 7);
    }
    brush.place(bricks, 5, 1, 5);
    brush.place(bricks, 5, 2, 5);
    brush.place(bricks, 5, 3, 5);
    brush.place(water(), 5, 4, 5);
}

/// Interior 2: a cobblestone frame under a plank loft, with a ladder and a chest above it.
void loftRoom(const StrongholdBrush& brush) {
    const std::int32_t cobble = strongholdPalette::cobblestone();
    for (std::int32_t z = 1; z <= 9; ++z) {
        brush.place(cobble, 1, 3, z);
        brush.place(cobble, 9, 3, z);
    }
    for (std::int32_t x = 1; x <= 9; ++x) {
        brush.place(cobble, x, 3, 1);
        brush.place(cobble, x, 3, 9);
    }
    brush.place(cobble, 5, 1, 4);
    brush.place(cobble, 5, 1, 6);
    brush.place(cobble, 5, 3, 4);
    brush.place(cobble, 5, 3, 6);
    brush.place(cobble, 4, 1, 5);
    brush.place(cobble, 6, 1, 5);
    brush.place(cobble, 4, 3, 5);
    brush.place(cobble, 6, 3, 5);
    for (std::int32_t y = 1; y <= 3; ++y) {
        brush.place(cobble, 4, y, 4);
        brush.place(cobble, 6, y, 4);
        brush.place(cobble, 4, y, 6);
        brush.place(cobble, 6, y, 6);
    }
    // parity: a wall torch with no facing set, which is the block's own default.
    brush.place(strongholdPalette::wallTorch("north"), 5, 3, 5);
    for (std::int32_t z = 2; z <= 8; ++z) {
        brush.place(oakPlanks(), 2, 3, z);
        brush.place(oakPlanks(), 3, 3, z);
        if (z <= 3 || z >= 7) {
            brush.place(oakPlanks(), 4, 3, z);
            brush.place(oakPlanks(), 5, 3, z);
            brush.place(oakPlanks(), 6, 3, z);
        }
        brush.place(oakPlanks(), 7, 3, z);
        brush.place(oakPlanks(), 8, 3, z);
    }
    brush.place(ladderWest(), 9, 1, 3);
    brush.place(ladderWest(), 9, 2, 3);
    brush.place(ladderWest(), 9, 3, 3);
    brush.createChest(3, 4, 8, "minecraft:chests/stronghold_crossing");
}

} // namespace

void strongholdRooms::prisonHall(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    brush.shell(0, 0, 0, 8, 4, 10);
    brush.door(piece.entryDoor, 1, 1, 0);
    brush.box(1, 1, 10, 3, 3, 10, strongholdPalette::caveAir());
    brush.shell(4, 1, 1, 4, 3, 1, false);
    brush.shell(4, 1, 3, 4, 3, 3, false);
    brush.shell(4, 1, 7, 4, 3, 7, false);
    brush.shell(4, 1, 9, 4, 3, 9, false);
    const std::int32_t northSouth = strongholdPalette::bars({"north", "south"});
    const std::int32_t northSouthEast = strongholdPalette::bars({"north", "south", "east"});
    const std::int32_t westEast = strongholdPalette::bars({"west", "east"});
    for (std::int32_t y = 1; y <= 3; ++y) {
        brush.place(northSouth, 4, y, 4);
        brush.place(northSouthEast, 4, y, 5);
        brush.place(northSouth, 4, y, 6);
        brush.place(westEast, 5, y, 5);
        brush.place(westEast, 6, y, 5);
        brush.place(westEast, 7, y, 5);
    }
    brush.place(northSouth, 4, 3, 2);
    brush.place(northSouth, 4, 3, 8);
    const std::int32_t lower = strongholdPalette::ironDoor("west", "lower");
    const std::int32_t upper = strongholdPalette::ironDoor("west", "upper");
    brush.place(lower, 4, 1, 2);
    brush.place(upper, 4, 2, 2);
    brush.place(lower, 4, 1, 8);
    brush.place(upper, 4, 2, 8);
}

void strongholdRooms::roomCrossing(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    const std::int32_t caveAir = strongholdPalette::caveAir();
    brush.shell(0, 0, 0, 10, 6, 10);
    brush.door(piece.entryDoor, 4, 1, 0);
    brush.box(4, 1, 10, 6, 3, 10, caveAir);
    brush.box(0, 1, 4, 0, 3, 6, caveAir);
    brush.box(10, 1, 4, 10, 3, 6, caveAir);
    switch (piece.roomType) {
    case 0:
        pillarRoom(brush);
        return;
    case 1:
        fountainRoom(brush);
        return;
    case 2:
        loftRoom(brush);
        return;
    default:
        return;
    }
}

void strongholdRooms::fiveCrossing(const StrongholdBrush& brush, const StrongholdPiece& piece) {
    const std::int32_t caveAir = strongholdPalette::caveAir();
    const std::int32_t slab = strongholdPalette::smoothStoneSlab();
    brush.shell(0, 0, 0, 9, 8, 10);
    brush.door(piece.entryDoor, 4, 3, 0);
    if (piece.leftLow) {
        brush.box(0, 3, 1, 0, 5, 3, caveAir);
    }
    if (piece.rightLow) {
        brush.box(9, 3, 1, 9, 5, 3, caveAir);
    }
    if (piece.leftHigh) {
        brush.box(0, 5, 7, 0, 7, 9, caveAir);
    }
    if (piece.rightHigh) {
        brush.box(9, 5, 7, 9, 7, 9, caveAir);
    }
    brush.box(5, 1, 10, 7, 3, 10, caveAir);
    brush.shell(1, 2, 1, 8, 2, 6, false);
    brush.shell(4, 1, 5, 4, 4, 9, false);
    brush.shell(8, 1, 5, 8, 4, 9, false);
    brush.shell(1, 4, 7, 3, 4, 9, false);
    brush.shell(1, 3, 5, 3, 3, 6, false);
    brush.box(1, 3, 4, 3, 3, 4, slab);
    brush.box(1, 4, 6, 3, 4, 6, slab);
    brush.shell(5, 1, 7, 7, 1, 8, false);
    brush.box(5, 1, 9, 7, 1, 9, slab);
    brush.box(5, 2, 7, 7, 2, 7, slab);
    brush.box(4, 5, 7, 4, 5, 9, slab);
    brush.box(8, 5, 7, 8, 5, 9, slab);
    brush.box(5, 5, 7, 7, 5, 9, doubleSlab());
    brush.place(strongholdPalette::wallTorch("south"), 6, 5, 6);
}

} // namespace basalt::server::world::structure
