package org.basaltmc.server.world.structure

import org.basaltmc.server.world.structure.StrongholdPalette.CAVE_AIR

/**
 * // parity: StrongholdPieces.Library. Two storeys when it had room for eleven blocks, one when it
 * had to settle for six, and the tall one carries the balcony, the ladder and the second chest.
 */
internal object StrongholdLibrary {
    private val PLANKS = StrongholdPalette.state("oak_planks")
    private val BOOKSHELF = StrongholdPalette.state("bookshelf")
    private val COBWEB = StrongholdPalette.state("cobweb")
    private val TORCH = StrongholdPalette.state("torch")
    private val LADDER_SOUTH = StrongholdPalette.state("ladder", mapOf("facing" to "south", "waterlogged" to "false"))
    private const val LOOT = "minecraft:chests/stronghold_library"

    private fun fence(vararg sides: String): Int =
        StrongholdPalette.state(
            "oak_fence",
            mapOf(
                "north" to sides.contains("north").toString(),
                "south" to sides.contains("south").toString(),
                "east" to sides.contains("east").toString(),
                "west" to sides.contains("west").toString(),
                "waterlogged" to "false",
            ),
        )

    fun place(
        brush: StrongholdBrush,
        piece: StrongholdPiece,
    ) {
        val height = if (piece.tall) 11 else 6
        brush.shell(0, 0, 0, 13, height - 1, 14)
        brush.door(piece.entryDoor, 4, 1, 0)
        // parity: the cobwebs draw over the whole 10 by 4 by 13 volume, one roll per block, before
        // any shelf goes in.
        brush.fill.generateMaybeBox(
            brush.random,
            0.07f,
            2,
            1,
            1,
            11,
            4,
            13,
            COBWEB,
            COBWEB,
            skipAir = false,
            onlyInside = false,
        )
        for (d in 1..13) {
            if ((d - 1) % 4 == 0) {
                brush.box(1, 1, d, 1, 4, d, PLANKS)
                brush.box(12, 1, d, 12, 4, d, PLANKS)
                brush.place(StrongholdPalette.wallTorch("east"), 2, 3, d)
                brush.place(StrongholdPalette.wallTorch("west"), 11, 3, d)
                if (piece.tall) {
                    brush.box(1, 6, d, 1, 9, d, PLANKS)
                    brush.box(12, 6, d, 12, 9, d, PLANKS)
                }
            } else {
                brush.box(1, 1, d, 1, 4, d, BOOKSHELF)
                brush.box(12, 1, d, 12, 4, d, BOOKSHELF)
                if (piece.tall) {
                    brush.box(1, 6, d, 1, 9, d, BOOKSHELF)
                    brush.box(12, 6, d, 12, 9, d, BOOKSHELF)
                }
            }
        }
        for (d in 3 until 12 step 2) {
            brush.box(3, 1, d, 4, 3, d, BOOKSHELF)
            brush.box(6, 1, d, 7, 3, d, BOOKSHELF)
            brush.box(9, 1, d, 10, 3, d, BOOKSHELF)
        }
        if (piece.tall) balcony(brush)
        brush.createChest(3, 3, 5, LOOT)
        if (piece.tall) {
            brush.place(CAVE_AIR, 12, 9, 1)
            brush.createChest(12, 8, 1, LOOT)
        }
    }

    /** The upper floor of the tall library: plank deck, fence railing, ladder and chandelier. */
    private fun balcony(brush: StrongholdBrush) {
        brush.box(1, 5, 1, 3, 5, 13, PLANKS)
        brush.box(10, 5, 1, 12, 5, 13, PLANKS)
        brush.box(4, 5, 1, 9, 5, 2, PLANKS)
        brush.box(4, 5, 12, 9, 5, 13, PLANKS)
        brush.place(PLANKS, 9, 5, 11)
        brush.place(PLANKS, 8, 5, 11)
        brush.place(PLANKS, 9, 5, 10)
        val weFence = fence("west", "east")
        val nsFence = fence("north", "south")
        brush.box(3, 6, 3, 3, 6, 11, nsFence)
        brush.box(10, 6, 3, 10, 6, 9, nsFence)
        brush.box(4, 6, 2, 9, 6, 2, weFence)
        brush.box(4, 6, 12, 7, 6, 12, weFence)
        brush.place(fence("north", "east"), 3, 6, 2)
        brush.place(fence("south", "east"), 3, 6, 12)
        brush.place(fence("north", "west"), 10, 6, 2)
        for (i in 0..2) {
            brush.place(fence("south", "west"), 8 + i, 6, 12 - i)
            if (i != 2) brush.place(fence("north", "east"), 8 + i, 6, 11 - i)
        }
        for (y in 1..7) brush.place(LADDER_SOUTH, 10, y, 13)
        val eFence = fence("east")
        val wFence = fence("west")
        brush.place(eFence, 6, 9, 7)
        brush.place(wFence, 7, 9, 7)
        brush.place(eFence, 6, 8, 7)
        brush.place(wFence, 7, 8, 7)
        val cross = fence("north", "south", "west", "east")
        brush.place(cross, 6, 7, 7)
        brush.place(cross, 7, 7, 7)
        brush.place(eFence, 5, 7, 7)
        brush.place(wFence, 8, 7, 7)
        brush.place(fence("east", "north"), 6, 7, 6)
        brush.place(fence("east", "south"), 6, 7, 8)
        brush.place(fence("west", "north"), 7, 7, 6)
        brush.place(fence("west", "south"), 7, 7, 8)
        brush.place(TORCH, 5, 8, 7)
        brush.place(TORCH, 8, 8, 7)
        brush.place(TORCH, 6, 8, 6)
        brush.place(TORCH, 6, 8, 8)
        brush.place(TORCH, 7, 8, 6)
        brush.place(TORCH, 7, 8, 8)
    }
}
