package org.basaltmc.server.world.structure

import org.basaltmc.server.random.BasaltRandom
import org.basaltmc.server.world.structure.MonumentPalette.BASE_BLACK
import org.basaltmc.server.world.structure.MonumentPalette.BASE_GRAY
import org.basaltmc.server.world.structure.MonumentPalette.BASE_LIGHT
import org.basaltmc.server.world.structure.MonumentPalette.GOLD
import org.basaltmc.server.world.structure.MonumentPalette.LAMP
import org.basaltmc.server.world.structure.MonumentPalette.WET_SPONGE

/**
 * The rooms that occupy a single lattice cell, plus the core the whole monument is built around.
 *
 * // parity: OceanMonument{Entry,Simple,SimpleTop,Core}Room.
 */
internal object MonumentSingleRooms {
    private fun opening(
        room: MonumentRoom,
        direction: StructureDirection,
    ): Boolean = room.hasOpening[direction.ordinal]

    private fun aboveGround(room: MonumentRoom): Boolean = room.index / 25 > 0

    /** The room the entrance corridor arrives in: three walls and whichever doorways survived. */
    fun entry(
        brush: MonumentBrush,
        room: MonumentRoom,
    ) {
        brush.box(0, 3, 0, 2, 3, 7, BASE_LIGHT)
        brush.box(5, 3, 0, 7, 3, 7, BASE_LIGHT)
        brush.box(0, 2, 0, 1, 2, 7, BASE_LIGHT)
        brush.box(6, 2, 0, 7, 2, 7, BASE_LIGHT)
        brush.box(0, 1, 0, 0, 1, 7, BASE_LIGHT)
        brush.box(7, 1, 0, 7, 1, 7, BASE_LIGHT)
        brush.box(0, 1, 7, 7, 3, 7, BASE_LIGHT)
        brush.box(1, 1, 0, 2, 3, 0, BASE_LIGHT)
        brush.box(5, 1, 0, 6, 3, 0, BASE_LIGHT)
        if (opening(room, StructureDirection.NORTH)) brush.waterBox(3, 1, 7, 4, 2, 7)
        if (opening(room, StructureDirection.WEST)) brush.waterBox(0, 1, 3, 1, 2, 4)
        if (opening(room, StructureDirection.EAST)) brush.waterBox(6, 1, 3, 7, 2, 4)
    }

    /**
     * // parity: OceanMonumentSimpleRoom. [design] was drawn when the room was chosen; the extra
     * draw for the middle pillar happens here, on the chunk's own random, and only for designs 1
     * and 2, so a room's interior is not decided by its design alone.
     */
    fun simple(
        brush: MonumentBrush,
        room: MonumentRoom,
        design: Int,
        random: BasaltRandom,
    ) {
        if (aboveGround(room)) brush.defaultFloor(0, 0, opening(room, StructureDirection.DOWN))
        if (room.connections[StructureDirection.UP.ordinal] == null) {
            brush.boxOnFillOnly(1, 4, 1, 6, 4, 6, BASE_GRAY)
        }
        val centrePillar =
            design != 0 &&
                random.nextBoolean() &&
                !opening(room, StructureDirection.DOWN) &&
                !opening(room, StructureDirection.UP) &&
                room.countOpenings() > 1
        when (design) {
            0 -> simpleCorners(brush, room)
            1 -> simpleColumns(brush, room)
            2 -> simpleBanded(brush, room)
        }
        if (centrePillar) {
            brush.box(3, 1, 3, 4, 1, 4, BASE_LIGHT)
            brush.box(3, 2, 3, 4, 2, 4, BASE_GRAY)
            brush.box(3, 3, 3, 4, 3, 4, BASE_LIGHT)
        }
    }

    /** Design 0: a lantern set into each of the four corners, and a lintel over every closed side. */
    private fun simpleCorners(
        brush: MonumentBrush,
        room: MonumentRoom,
    ) {
        brush.box(0, 1, 0, 2, 1, 2, BASE_LIGHT)
        brush.box(0, 3, 0, 2, 3, 2, BASE_LIGHT)
        brush.box(0, 2, 0, 0, 2, 2, BASE_GRAY)
        brush.box(1, 2, 0, 2, 2, 0, BASE_GRAY)
        brush.place(LAMP, 1, 2, 1)
        brush.box(5, 1, 0, 7, 1, 2, BASE_LIGHT)
        brush.box(5, 3, 0, 7, 3, 2, BASE_LIGHT)
        brush.box(7, 2, 0, 7, 2, 2, BASE_GRAY)
        brush.box(5, 2, 0, 6, 2, 0, BASE_GRAY)
        brush.place(LAMP, 6, 2, 1)
        brush.box(0, 1, 5, 2, 1, 7, BASE_LIGHT)
        brush.box(0, 3, 5, 2, 3, 7, BASE_LIGHT)
        brush.box(0, 2, 5, 0, 2, 7, BASE_GRAY)
        brush.box(1, 2, 7, 2, 2, 7, BASE_GRAY)
        brush.place(LAMP, 1, 2, 6)
        brush.box(5, 1, 5, 7, 1, 7, BASE_LIGHT)
        brush.box(5, 3, 5, 7, 3, 7, BASE_LIGHT)
        brush.box(7, 2, 5, 7, 2, 7, BASE_GRAY)
        brush.box(5, 2, 7, 6, 2, 7, BASE_GRAY)
        brush.place(LAMP, 6, 2, 6)
        if (opening(room, StructureDirection.SOUTH)) {
            brush.box(3, 3, 0, 4, 3, 0, BASE_LIGHT)
        } else {
            brush.box(3, 3, 0, 4, 3, 1, BASE_LIGHT)
            brush.box(3, 2, 0, 4, 2, 0, BASE_GRAY)
            brush.box(3, 1, 0, 4, 1, 1, BASE_LIGHT)
        }
        if (opening(room, StructureDirection.NORTH)) {
            brush.box(3, 3, 7, 4, 3, 7, BASE_LIGHT)
        } else {
            brush.box(3, 3, 6, 4, 3, 7, BASE_LIGHT)
            brush.box(3, 2, 7, 4, 2, 7, BASE_GRAY)
            brush.box(3, 1, 6, 4, 1, 7, BASE_LIGHT)
        }
        if (opening(room, StructureDirection.WEST)) {
            brush.box(0, 3, 3, 0, 3, 4, BASE_LIGHT)
        } else {
            brush.box(0, 3, 3, 1, 3, 4, BASE_LIGHT)
            brush.box(0, 2, 3, 0, 2, 4, BASE_GRAY)
            brush.box(0, 1, 3, 1, 1, 4, BASE_LIGHT)
        }
        if (opening(room, StructureDirection.EAST)) {
            brush.box(7, 3, 3, 7, 3, 4, BASE_LIGHT)
        } else {
            brush.box(6, 3, 3, 7, 3, 4, BASE_LIGHT)
            brush.box(7, 2, 3, 7, 2, 4, BASE_GRAY)
            brush.box(6, 1, 3, 7, 1, 4, BASE_LIGHT)
        }
    }

    /** Design 1: four free-standing lantern columns, and a solid wall wherever there is no doorway. */
    private fun simpleColumns(
        brush: MonumentBrush,
        room: MonumentRoom,
    ) {
        brush.box(2, 1, 2, 2, 3, 2, BASE_LIGHT)
        brush.box(2, 1, 5, 2, 3, 5, BASE_LIGHT)
        brush.box(5, 1, 5, 5, 3, 5, BASE_LIGHT)
        brush.box(5, 1, 2, 5, 3, 2, BASE_LIGHT)
        brush.place(LAMP, 2, 2, 2)
        brush.place(LAMP, 2, 2, 5)
        brush.place(LAMP, 5, 2, 5)
        brush.place(LAMP, 5, 2, 2)
        brush.box(0, 1, 0, 1, 3, 0, BASE_LIGHT)
        brush.box(0, 1, 1, 0, 3, 1, BASE_LIGHT)
        brush.box(0, 1, 7, 1, 3, 7, BASE_LIGHT)
        brush.box(0, 1, 6, 0, 3, 6, BASE_LIGHT)
        brush.box(6, 1, 7, 7, 3, 7, BASE_LIGHT)
        brush.box(7, 1, 6, 7, 3, 6, BASE_LIGHT)
        brush.box(6, 1, 0, 7, 3, 0, BASE_LIGHT)
        brush.box(7, 1, 1, 7, 3, 1, BASE_LIGHT)
        brush.place(BASE_GRAY, 1, 2, 0)
        brush.place(BASE_GRAY, 0, 2, 1)
        brush.place(BASE_GRAY, 1, 2, 7)
        brush.place(BASE_GRAY, 0, 2, 6)
        brush.place(BASE_GRAY, 6, 2, 7)
        brush.place(BASE_GRAY, 7, 2, 6)
        brush.place(BASE_GRAY, 6, 2, 0)
        brush.place(BASE_GRAY, 7, 2, 1)
        if (!opening(room, StructureDirection.SOUTH)) {
            brush.box(1, 3, 0, 6, 3, 0, BASE_LIGHT)
            brush.box(1, 2, 0, 6, 2, 0, BASE_GRAY)
            brush.box(1, 1, 0, 6, 1, 0, BASE_LIGHT)
        }
        if (!opening(room, StructureDirection.NORTH)) {
            brush.box(1, 3, 7, 6, 3, 7, BASE_LIGHT)
            brush.box(1, 2, 7, 6, 2, 7, BASE_GRAY)
            brush.box(1, 1, 7, 6, 1, 7, BASE_LIGHT)
        }
        if (!opening(room, StructureDirection.WEST)) {
            brush.box(0, 3, 1, 0, 3, 6, BASE_LIGHT)
            brush.box(0, 2, 1, 0, 2, 6, BASE_GRAY)
            brush.box(0, 1, 1, 0, 1, 6, BASE_LIGHT)
        }
        if (!opening(room, StructureDirection.EAST)) {
            brush.box(7, 3, 1, 7, 3, 6, BASE_LIGHT)
            brush.box(7, 2, 1, 7, 2, 6, BASE_GRAY)
            brush.box(7, 1, 1, 7, 1, 6, BASE_LIGHT)
        }
    }

    /** Design 2 and the shell of the top room: three courses, the middle one dark. */
    private fun bandedShell(brush: MonumentBrush) {
        for ((y, block) in listOf(1 to BASE_LIGHT, 2 to BASE_BLACK, 3 to BASE_LIGHT)) {
            brush.box(0, y, 0, 0, y, 7, block)
            brush.box(7, y, 0, 7, y, 7, block)
            brush.box(1, y, 0, 6, y, 0, block)
            brush.box(1, y, 7, 6, y, 7, block)
        }
        brush.box(0, 1, 3, 0, 2, 4, BASE_BLACK)
        brush.box(7, 1, 3, 7, 2, 4, BASE_BLACK)
        brush.box(3, 1, 0, 4, 2, 0, BASE_BLACK)
        brush.box(3, 1, 7, 4, 2, 7, BASE_BLACK)
    }

    private fun simpleBanded(
        brush: MonumentBrush,
        room: MonumentRoom,
    ) {
        bandedShell(brush)
        if (opening(room, StructureDirection.SOUTH)) brush.waterBox(3, 1, 0, 4, 2, 0)
        if (opening(room, StructureDirection.NORTH)) brush.waterBox(3, 1, 7, 4, 2, 7)
        if (opening(room, StructureDirection.WEST)) brush.waterBox(0, 1, 3, 0, 2, 4)
        if (opening(room, StructureDirection.EAST)) brush.waterBox(7, 1, 3, 7, 2, 4)
    }

    /** // parity: OceanMonumentSimpleTopRoom, the sealed cell that grows the sponges. */
    fun simpleTop(
        brush: MonumentBrush,
        room: MonumentRoom,
        random: BasaltRandom,
    ) {
        if (aboveGround(room)) brush.defaultFloor(0, 0, opening(room, StructureDirection.DOWN))
        if (room.connections[StructureDirection.UP.ordinal] == null) {
            brush.boxOnFillOnly(1, 4, 1, 6, 4, 6, BASE_GRAY)
        }
        for (x in 1..6) {
            for (z in 1..6) {
                if (random.nextInt(3) == 0) continue
                val y0 = 2 + if (random.nextInt(4) == 0) 0 else 1
                brush.box(x, y0, z, x, 3, z, WET_SPONGE)
            }
        }
        bandedShell(brush)
        if (opening(room, StructureDirection.SOUTH)) brush.waterBox(3, 1, 0, 4, 2, 0)
    }

    /** // parity: OceanMonumentCoreRoom, the two by two by two block with the gold in the middle. */
    fun core(brush: MonumentBrush) {
        brush.boxOnFillOnly(1, 8, 0, 14, 8, 14, BASE_GRAY)
        brush.box(0, 7, 0, 0, 7, 15, BASE_LIGHT)
        brush.box(15, 7, 0, 15, 7, 15, BASE_LIGHT)
        brush.box(1, 7, 0, 15, 7, 0, BASE_LIGHT)
        brush.box(1, 7, 15, 14, 7, 15, BASE_LIGHT)
        for (y in 1..6) {
            val block = if (y == 2 || y == 6) BASE_GRAY else BASE_LIGHT
            for (x in 0..15 step 15) {
                brush.box(x, y, 0, x, y, 1, block)
                brush.box(x, y, 6, x, y, 9, block)
                brush.box(x, y, 14, x, y, 15, block)
            }
            brush.box(1, y, 0, 1, y, 0, block)
            brush.box(6, y, 0, 9, y, 0, block)
            brush.box(14, y, 0, 14, y, 0, block)
            brush.box(1, y, 15, 14, y, 15, block)
        }
        brush.box(6, 3, 6, 9, 6, 9, BASE_BLACK)
        brush.box(7, 4, 7, 8, 5, 8, GOLD)
        for (y in 3..6 step 3) {
            for (x in 6..9 step 3) {
                brush.place(LAMP, x, y, 6)
                brush.place(LAMP, x, y, 9)
            }
        }
        brush.box(5, 1, 6, 5, 2, 6, BASE_LIGHT)
        brush.box(5, 1, 9, 5, 2, 9, BASE_LIGHT)
        brush.box(10, 1, 6, 10, 2, 6, BASE_LIGHT)
        brush.box(10, 1, 9, 10, 2, 9, BASE_LIGHT)
        brush.box(6, 1, 5, 6, 2, 5, BASE_LIGHT)
        brush.box(9, 1, 5, 9, 2, 5, BASE_LIGHT)
        brush.box(6, 1, 10, 6, 2, 10, BASE_LIGHT)
        brush.box(9, 1, 10, 9, 2, 10, BASE_LIGHT)
        brush.box(5, 2, 5, 5, 6, 5, BASE_LIGHT)
        brush.box(5, 2, 10, 5, 6, 10, BASE_LIGHT)
        brush.box(10, 2, 5, 10, 6, 5, BASE_LIGHT)
        brush.box(10, 2, 10, 10, 6, 10, BASE_LIGHT)
        brush.box(5, 7, 1, 5, 7, 6, BASE_LIGHT)
        brush.box(10, 7, 1, 10, 7, 6, BASE_LIGHT)
        brush.box(5, 7, 9, 5, 7, 14, BASE_LIGHT)
        brush.box(10, 7, 9, 10, 7, 14, BASE_LIGHT)
        brush.box(1, 7, 5, 6, 7, 5, BASE_LIGHT)
        brush.box(1, 7, 10, 6, 7, 10, BASE_LIGHT)
        brush.box(9, 7, 5, 14, 7, 5, BASE_LIGHT)
        brush.box(9, 7, 10, 14, 7, 10, BASE_LIGHT)
        brush.box(2, 1, 2, 2, 1, 3, BASE_LIGHT)
        brush.box(3, 1, 2, 3, 1, 2, BASE_LIGHT)
        brush.box(13, 1, 2, 13, 1, 3, BASE_LIGHT)
        brush.box(12, 1, 2, 12, 1, 2, BASE_LIGHT)
        brush.box(2, 1, 12, 2, 1, 13, BASE_LIGHT)
        brush.box(3, 1, 13, 3, 1, 13, BASE_LIGHT)
        brush.box(13, 1, 12, 13, 1, 13, BASE_LIGHT)
        brush.box(12, 1, 13, 12, 1, 13, BASE_LIGHT)
    }
}
