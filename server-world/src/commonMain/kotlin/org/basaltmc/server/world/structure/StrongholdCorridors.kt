package org.basaltmc.server.world.structure

import org.basaltmc.server.world.structure.StrongholdPalette.CAVE_AIR
import org.basaltmc.server.world.structure.StrongholdPalette.SMOOTH_STONE_SLAB
import org.basaltmc.server.world.structure.StrongholdPalette.STONE_BRICKS

/**
 * The pieces that only carry the maze along. // parity: StrongholdPieces' Straight,
 * StraightStairsDown, StairsDown, Turn and FillerCorridor.
 */
internal object StrongholdCorridors {
    /** // parity: StairsDown, which is also the start piece: a spiral of brick and slab. */
    fun stairsDown(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 4, 10, 4)
        brush.door(piece.entryDoor, 1, 7, 0)
        brush.door(StrongholdDoor.OPENING, 1, 1, 4)
        brush.place(STONE_BRICKS, 2, 6, 1)
        brush.place(STONE_BRICKS, 1, 5, 1)
        brush.place(SMOOTH_STONE_SLAB, 1, 6, 1)
        brush.place(STONE_BRICKS, 1, 5, 2)
        brush.place(STONE_BRICKS, 1, 4, 3)
        brush.place(SMOOTH_STONE_SLAB, 1, 5, 3)
        brush.place(STONE_BRICKS, 2, 4, 3)
        brush.place(STONE_BRICKS, 3, 3, 3)
        brush.place(SMOOTH_STONE_SLAB, 3, 4, 3)
        brush.place(STONE_BRICKS, 3, 3, 2)
        brush.place(STONE_BRICKS, 3, 2, 1)
        brush.place(SMOOTH_STONE_SLAB, 3, 3, 1)
        brush.place(STONE_BRICKS, 2, 2, 1)
        brush.place(STONE_BRICKS, 1, 1, 1)
        brush.place(SMOOTH_STONE_SLAB, 1, 2, 1)
        brush.place(STONE_BRICKS, 1, 1, 2)
        brush.place(SMOOTH_STONE_SLAB, 1, 1, 3)
    }

    /** // parity: StraightStairsDown, the long flight of cobblestone stairs. */
    fun straightStairsDown(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 4, 10, 7)
        brush.door(piece.entryDoor, 1, 7, 0)
        brush.door(StrongholdDoor.OPENING, 1, 1, 7)
        val stairs = StrongholdPalette.stairs("cobblestone_stairs", "south")
        for (i in 0 until 6) {
            brush.place(stairs, 1, 6 - i, 1 + i)
            brush.place(stairs, 2, 6 - i, 1 + i)
            brush.place(stairs, 3, 6 - i, 1 + i)
            if (i < 5) {
                brush.place(STONE_BRICKS, 1, 5 - i, 1 + i)
                brush.place(STONE_BRICKS, 2, 5 - i, 1 + i)
                brush.place(STONE_BRICKS, 3, 5 - i, 1 + i)
            }
        }
    }

    /** // parity: Straight. The four torch draws happen whether or not a torch lands. */
    fun straight(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 4, 4, 6)
        brush.door(piece.entryDoor, 1, 1, 0)
        brush.door(StrongholdDoor.OPENING, 1, 1, 6)
        val east = StrongholdPalette.wallTorch("east")
        val west = StrongholdPalette.wallTorch("west")
        brush.maybe(0.1f, 1, 2, 1, east)
        brush.maybe(0.1f, 3, 2, 1, west)
        brush.maybe(0.1f, 1, 2, 5, east)
        brush.maybe(0.1f, 3, 2, 5, west)
        if (piece.leftChild) brush.box(0, 1, 2, 0, 3, 4, CAVE_AIR)
        if (piece.rightChild) brush.box(4, 1, 2, 4, 3, 4, CAVE_AIR)
    }

    /**
     * // parity: LeftTurn and RightTurn, which are the same room with the opening on the other side.
     * Which side that is depends on the facing, and the two classes disagree on it, so it is not a
     * plain mirror.
     */
    fun turn(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 4, 4, 4)
        brush.door(piece.entryDoor, 1, 1, 0)
        val flipped =
            piece.orientation != StructureDirection.NORTH && piece.orientation != StructureDirection.EAST
        val left = piece.kind == StrongholdPieceKind.LEFT_TURN
        val onEast = if (left) flipped else !flipped
        if (onEast) brush.box(4, 1, 1, 4, 3, 3, CAVE_AIR) else brush.box(0, 1, 1, 0, 3, 3, CAVE_AIR)
    }

    /** // parity: ChestCorridor, whose chest is the only thing that makes it not a plain corridor. */
    fun chestCorridor(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 4, 4, 6)
        brush.door(piece.entryDoor, 1, 1, 0)
        brush.door(StrongholdDoor.OPENING, 1, 1, 6)
        brush.box(3, 1, 2, 3, 1, 4, STONE_BRICKS)
        brush.place(StrongholdPalette.STONE_BRICK_SLAB, 3, 1, 1)
        brush.place(StrongholdPalette.STONE_BRICK_SLAB, 3, 1, 5)
        brush.place(StrongholdPalette.STONE_BRICK_SLAB, 3, 2, 2)
        brush.place(StrongholdPalette.STONE_BRICK_SLAB, 3, 2, 4)
        for (z in 2..4) brush.place(StrongholdPalette.STONE_BRICK_SLAB, 2, 1, z)
        brush.createChest(3, 2, 3, "minecraft:chests/stronghold_corridor")
    }

    /** // parity: FillerCorridor, a plain brick tube with no shell selector and no draws at all. */
    fun fillerCorridor(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        for (z in 0 until piece.steps) {
            for (x in 0..4) brush.place(STONE_BRICKS, x, 0, z)
            for (y in 1..3) {
                brush.place(STONE_BRICKS, 0, y, z)
                brush.place(CAVE_AIR, 1, y, z)
                brush.place(CAVE_AIR, 2, y, z)
                brush.place(CAVE_AIR, 3, y, z)
                brush.place(STONE_BRICKS, 4, y, z)
            }
            for (x in 0..4) brush.place(STONE_BRICKS, x, 4, z)
        }
    }
}
