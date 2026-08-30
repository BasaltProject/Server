package org.basaltmc.server.world.structure

import org.basaltmc.server.world.structure.StrongholdPalette.CAVE_AIR
import org.basaltmc.server.world.structure.StrongholdPalette.COBBLESTONE
import org.basaltmc.server.world.structure.StrongholdPalette.SMOOTH_STONE_SLAB
import org.basaltmc.server.world.structure.StrongholdPalette.STONE_BRICKS

/** The three wide rooms. // parity: StrongholdPieces' PrisonHall, RoomCrossing and FiveCrossing. */
internal object StrongholdRooms {
    private val OAK_PLANKS = StrongholdPalette.state("oak_planks")
    private val WATER = StrongholdPalette.state("water")
    private val DOUBLE_SLAB =
        StrongholdPalette.state(
            "smooth_stone_slab",
            mapOf(
                "type" to "double",
                "waterlogged" to "false",
            ),
        )
    private val LADDER_WEST = StrongholdPalette.state("ladder", mapOf("facing" to "west", "waterlogged" to "false"))

    /** // parity: PrisonHall, whose four cells are iron bars and iron doors. */
    fun prisonHall(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 8, 4, 10)
        brush.door(piece.entryDoor, 1, 1, 0)
        brush.box(1, 1, 10, 3, 3, 10, CAVE_AIR)
        brush.shell(4, 1, 1, 4, 3, 1, skipAir = false)
        brush.shell(4, 1, 3, 4, 3, 3, skipAir = false)
        brush.shell(4, 1, 7, 4, 3, 7, skipAir = false)
        brush.shell(4, 1, 9, 4, 3, 9, skipAir = false)
        val northSouth = StrongholdPalette.bars("north", "south")
        val northSouthEast = StrongholdPalette.bars("north", "south", "east")
        val westEast = StrongholdPalette.bars("west", "east")
        for (y in 1..3) {
            brush.place(northSouth, 4, y, 4)
            brush.place(northSouthEast, 4, y, 5)
            brush.place(northSouth, 4, y, 6)
            brush.place(westEast, 5, y, 5)
            brush.place(westEast, 6, y, 5)
            brush.place(westEast, 7, y, 5)
        }
        brush.place(northSouth, 4, 3, 2)
        brush.place(northSouth, 4, 3, 8)
        val lower = StrongholdPalette.ironDoor("west", "lower")
        val upper = StrongholdPalette.ironDoor("west", "upper")
        brush.place(lower, 4, 1, 2)
        brush.place(upper, 4, 2, 2)
        brush.place(lower, 4, 1, 8)
        brush.place(upper, 4, 2, 8)
    }

    /** // parity: RoomCrossing. Its five interiors; the last two are simply an empty room. */
    fun roomCrossing(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 10, 6, 10)
        brush.door(piece.entryDoor, 4, 1, 0)
        brush.box(4, 1, 10, 6, 3, 10, CAVE_AIR)
        brush.box(0, 1, 4, 0, 3, 6, CAVE_AIR)
        brush.box(10, 1, 4, 10, 3, 6, CAVE_AIR)
        when (piece.roomType) {
            0 -> pillarRoom(brush)
            1 -> fountainRoom(brush)
            2 -> loftRoom(brush)
        }
    }

    /** Interior 0: a lit pillar ringed with slabs. */
    private fun pillarRoom(brush: StrongholdBrush) {
        brush.place(STONE_BRICKS, 5, 1, 5)
        brush.place(STONE_BRICKS, 5, 2, 5)
        brush.place(STONE_BRICKS, 5, 3, 5)
        brush.place(StrongholdPalette.wallTorch("west"), 4, 3, 5)
        brush.place(StrongholdPalette.wallTorch("east"), 6, 3, 5)
        brush.place(StrongholdPalette.wallTorch("south"), 5, 3, 4)
        brush.place(StrongholdPalette.wallTorch("north"), 5, 3, 6)
        brush.place(SMOOTH_STONE_SLAB, 4, 1, 4)
        brush.place(SMOOTH_STONE_SLAB, 4, 1, 5)
        brush.place(SMOOTH_STONE_SLAB, 4, 1, 6)
        brush.place(SMOOTH_STONE_SLAB, 6, 1, 4)
        brush.place(SMOOTH_STONE_SLAB, 6, 1, 5)
        brush.place(SMOOTH_STONE_SLAB, 6, 1, 6)
        brush.place(SMOOTH_STONE_SLAB, 5, 1, 4)
        brush.place(SMOOTH_STONE_SLAB, 5, 1, 6)
    }

    /** Interior 1: a brick ring around a pillar with water on top. */
    private fun fountainRoom(brush: StrongholdBrush) {
        for (i in 0 until 5) {
            brush.place(STONE_BRICKS, 3, 1, 3 + i)
            brush.place(STONE_BRICKS, 7, 1, 3 + i)
            brush.place(STONE_BRICKS, 3 + i, 1, 3)
            brush.place(STONE_BRICKS, 3 + i, 1, 7)
        }
        brush.place(STONE_BRICKS, 5, 1, 5)
        brush.place(STONE_BRICKS, 5, 2, 5)
        brush.place(STONE_BRICKS, 5, 3, 5)
        brush.place(WATER, 5, 4, 5)
    }

    /** Interior 2: a cobblestone frame under a plank loft, with a ladder and a chest above it. */
    private fun loftRoom(brush: StrongholdBrush) {
        for (z in 1..9) {
            brush.place(COBBLESTONE, 1, 3, z)
            brush.place(COBBLESTONE, 9, 3, z)
        }
        for (x in 1..9) {
            brush.place(COBBLESTONE, x, 3, 1)
            brush.place(COBBLESTONE, x, 3, 9)
        }
        brush.place(COBBLESTONE, 5, 1, 4)
        brush.place(COBBLESTONE, 5, 1, 6)
        brush.place(COBBLESTONE, 5, 3, 4)
        brush.place(COBBLESTONE, 5, 3, 6)
        brush.place(COBBLESTONE, 4, 1, 5)
        brush.place(COBBLESTONE, 6, 1, 5)
        brush.place(COBBLESTONE, 4, 3, 5)
        brush.place(COBBLESTONE, 6, 3, 5)
        for (y in 1..3) {
            brush.place(COBBLESTONE, 4, y, 4)
            brush.place(COBBLESTONE, 6, y, 4)
            brush.place(COBBLESTONE, 4, y, 6)
            brush.place(COBBLESTONE, 6, y, 6)
        }
        // parity: a wall torch with no facing set, which is the block's own default.
        brush.place(StrongholdPalette.wallTorch("north"), 5, 3, 5)
        for (z in 2..8) {
            brush.place(OAK_PLANKS, 2, 3, z)
            brush.place(OAK_PLANKS, 3, 3, z)
            if (z <= 3 || z >= 7) {
                brush.place(OAK_PLANKS, 4, 3, z)
                brush.place(OAK_PLANKS, 5, 3, z)
                brush.place(OAK_PLANKS, 6, 3, z)
            }
            brush.place(OAK_PLANKS, 7, 3, z)
            brush.place(OAK_PLANKS, 8, 3, z)
        }
        brush.place(LADDER_WEST, 9, 1, 3)
        brush.place(LADDER_WEST, 9, 2, 3)
        brush.place(LADDER_WEST, 9, 3, 3)
        brush.createChest(3, 4, 8, "minecraft:chests/stronghold_crossing")
    }

    /** // parity: FiveCrossing, the two-storey junction with up to four side openings. */
    fun fiveCrossing(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        brush.shell(0, 0, 0, 9, 8, 10)
        brush.door(piece.entryDoor, 4, 3, 0)
        if (piece.leftLow) brush.box(0, 3, 1, 0, 5, 3, CAVE_AIR)
        if (piece.rightLow) brush.box(9, 3, 1, 9, 5, 3, CAVE_AIR)
        if (piece.leftHigh) brush.box(0, 5, 7, 0, 7, 9, CAVE_AIR)
        if (piece.rightHigh) brush.box(9, 5, 7, 9, 7, 9, CAVE_AIR)
        brush.box(5, 1, 10, 7, 3, 10, CAVE_AIR)
        brush.shell(1, 2, 1, 8, 2, 6, skipAir = false)
        brush.shell(4, 1, 5, 4, 4, 9, skipAir = false)
        brush.shell(8, 1, 5, 8, 4, 9, skipAir = false)
        brush.shell(1, 4, 7, 3, 4, 9, skipAir = false)
        brush.shell(1, 3, 5, 3, 3, 6, skipAir = false)
        brush.box(1, 3, 4, 3, 3, 4, SMOOTH_STONE_SLAB)
        brush.box(1, 4, 6, 3, 4, 6, SMOOTH_STONE_SLAB)
        brush.shell(5, 1, 7, 7, 1, 8, skipAir = false)
        brush.box(5, 1, 9, 7, 1, 9, SMOOTH_STONE_SLAB)
        brush.box(5, 2, 7, 7, 2, 7, SMOOTH_STONE_SLAB)
        brush.box(4, 5, 7, 4, 5, 9, SMOOTH_STONE_SLAB)
        brush.box(8, 5, 7, 8, 5, 9, SMOOTH_STONE_SLAB)
        brush.box(5, 5, 7, 7, 5, 9, DOUBLE_SLAB)
        brush.place(StrongholdPalette.wallTorch("south"), 6, 5, 6)
    }
}
