package org.basaltmc.server.world.structure

/**
 * // parity: StrongholdPieces.PortalRoom. The one room a stronghold is built for, and the only one
 * whose shell is drawn without skipping air, so it always closes even inside a cave.
 */
internal object StrongholdPortalRoom {
    private val LAVA = StrongholdPalette.state("lava")
    private val END_PORTAL = StrongholdPalette.state("end_portal")

    private fun frame(
        facing: String,
        hasEye: Boolean,
    ): Int = StrongholdPalette.state("end_portal_frame", mapOf("facing" to facing, "eye" to hasEye.toString()))

    fun place(brush: StrongholdBrush) {
        brush.shell(0, 0, 0, 10, 7, 15, skipAir = false)
        brush.door(StrongholdDoor.GRATES, 4, 1, 0)
        brush.shell(1, 6, 1, 1, 6, 14, skipAir = false)
        brush.shell(9, 6, 1, 9, 6, 14, skipAir = false)
        brush.shell(2, 6, 1, 8, 6, 2, skipAir = false)
        brush.shell(2, 6, 14, 8, 6, 14, skipAir = false)
        brush.shell(1, 1, 1, 2, 1, 4, skipAir = false)
        brush.shell(8, 1, 1, 9, 1, 4, skipAir = false)
        brush.box(1, 1, 1, 1, 1, 3, LAVA)
        brush.box(9, 1, 1, 9, 1, 3, LAVA)
        brush.shell(3, 1, 8, 7, 1, 12, skipAir = false)
        brush.box(4, 1, 9, 6, 1, 11, LAVA)
        val northSouth = StrongholdPalette.bars("north", "south")
        val westEast = StrongholdPalette.bars("west", "east")
        for (z in 3 until 14 step 2) {
            brush.box(0, 3, z, 0, 4, z, northSouth)
            brush.box(10, 3, z, 10, 4, z, northSouth)
        }
        for (x in 2 until 9 step 2) {
            brush.box(x, 3, 15, x, 4, 15, westEast)
        }
        val stairs = StrongholdPalette.stairs("stone_brick_stairs", "north")
        brush.shell(4, 1, 5, 6, 1, 7, skipAir = false)
        brush.shell(4, 2, 6, 6, 2, 7, skipAir = false)
        brush.shell(4, 3, 7, 6, 3, 7, skipAir = false)
        for (x in 4..6) {
            brush.place(stairs, x, 1, 4)
            brush.place(stairs, x, 2, 5)
            brush.place(stairs, x, 3, 6)
        }
        // parity: the twelve eyes are drawn before any frame is placed, and all twelve are drawn
        // even when the first already fails, which is what makes a filled portal so rare.
        val eyes = BooleanArray(12) { brush.random.nextFloat() > 0.9f }
        val allEyes = eyes.all { it }
        brush.place(frame("north", eyes[0]), 4, 3, 8)
        brush.place(frame("north", eyes[1]), 5, 3, 8)
        brush.place(frame("north", eyes[2]), 6, 3, 8)
        brush.place(frame("south", eyes[3]), 4, 3, 12)
        brush.place(frame("south", eyes[4]), 5, 3, 12)
        brush.place(frame("south", eyes[5]), 6, 3, 12)
        brush.place(frame("east", eyes[6]), 3, 3, 9)
        brush.place(frame("east", eyes[7]), 3, 3, 10)
        brush.place(frame("east", eyes[8]), 3, 3, 11)
        brush.place(frame("west", eyes[9]), 7, 3, 9)
        brush.place(frame("west", eyes[10]), 7, 3, 10)
        brush.place(frame("west", eyes[11]), 7, 3, 11)
        if (allEyes) {
            for (z in 9..11) {
                for (x in 4..6) brush.place(END_PORTAL, x, 3, z)
            }
        }
        brush.silverfishSpawner(5, 3, 6)
    }
}
