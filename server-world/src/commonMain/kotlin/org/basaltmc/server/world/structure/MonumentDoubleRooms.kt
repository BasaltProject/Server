package org.basaltmc.server.world.structure

import org.basaltmc.server.world.structure.MonumentPalette.BASE_BLACK
import org.basaltmc.server.world.structure.MonumentPalette.BASE_GRAY
import org.basaltmc.server.world.structure.MonumentPalette.BASE_LIGHT
import org.basaltmc.server.world.structure.MonumentPalette.LAMP

/**
 * The five rooms that span two lattice cells, all the same shape: floors, cap, shell, then a doorway
 * per side that kept one. // parity: OceanMonumentDouble{X,XY,Y,YZ,Z}Room.
 */
internal object MonumentDoubleRooms {
    private fun opening(
        room: MonumentRoom,
        direction: StructureDirection,
    ): Boolean = room.hasOpening[direction.ordinal]

    private fun up(room: MonumentRoom): MonumentRoom? = room.connections[StructureDirection.UP.ordinal]

    /** A room on the ground floor sits on the monument's own base, so it lays no floor of its own. */
    private fun aboveGround(room: MonumentRoom): Boolean = room.index / 25 > 0

    fun doubleX(
        brush: MonumentBrush,
        west: MonumentRoom,
    ) {
        val east = west.connections[StructureDirection.EAST.ordinal] ?: return
        if (aboveGround(west)) {
            brush.defaultFloor(8, 0, opening(east, StructureDirection.DOWN))
            brush.defaultFloor(0, 0, opening(west, StructureDirection.DOWN))
        }
        if (up(west) == null) brush.boxOnFillOnly(1, 4, 1, 7, 4, 6, BASE_GRAY)
        if (up(east) == null) brush.boxOnFillOnly(8, 4, 1, 14, 4, 6, BASE_GRAY)
        for ((y, block) in listOf(3 to BASE_LIGHT, 2 to BASE_GRAY, 1 to BASE_LIGHT)) {
            brush.box(0, y, 0, 0, y, 7, block)
            brush.box(15, y, 0, 15, y, 7, block)
            brush.box(1, y, 0, 15, y, 0, block)
            brush.box(1, y, 7, 14, y, 7, block)
        }
        brush.box(5, 1, 0, 10, 1, 4, BASE_LIGHT)
        brush.box(6, 2, 0, 9, 2, 3, BASE_GRAY)
        brush.box(5, 3, 0, 10, 3, 4, BASE_LIGHT)
        brush.place(LAMP, 6, 2, 3)
        brush.place(LAMP, 9, 2, 3)
        lowerDoors(brush, west, east)
    }

    fun doubleXy(
        brush: MonumentBrush,
        west: MonumentRoom,
    ) {
        val east = west.connections[StructureDirection.EAST.ordinal] ?: return
        val westUp = up(west) ?: return
        val eastUp = up(east) ?: return
        if (aboveGround(west)) {
            brush.defaultFloor(8, 0, opening(east, StructureDirection.DOWN))
            brush.defaultFloor(0, 0, opening(west, StructureDirection.DOWN))
        }
        if (up(westUp) == null) brush.boxOnFillOnly(1, 8, 1, 7, 8, 6, BASE_GRAY)
        if (up(eastUp) == null) brush.boxOnFillOnly(8, 8, 1, 14, 8, 6, BASE_GRAY)
        for (y in 1..7) {
            val block = if (y == 2 || y == 6) BASE_GRAY else BASE_LIGHT
            brush.box(0, y, 0, 0, y, 7, block)
            brush.box(15, y, 0, 15, y, 7, block)
            brush.box(1, y, 0, 15, y, 0, block)
            brush.box(1, y, 7, 14, y, 7, block)
        }
        brush.box(2, 1, 3, 2, 7, 4, BASE_LIGHT)
        brush.box(3, 1, 2, 4, 7, 2, BASE_LIGHT)
        brush.box(3, 1, 5, 4, 7, 5, BASE_LIGHT)
        brush.box(13, 1, 3, 13, 7, 4, BASE_LIGHT)
        brush.box(11, 1, 2, 12, 7, 2, BASE_LIGHT)
        brush.box(11, 1, 5, 12, 7, 5, BASE_LIGHT)
        brush.box(5, 1, 3, 5, 3, 4, BASE_LIGHT)
        brush.box(10, 1, 3, 10, 3, 4, BASE_LIGHT)
        brush.box(5, 7, 2, 10, 7, 5, BASE_LIGHT)
        brush.box(5, 5, 2, 5, 7, 2, BASE_LIGHT)
        brush.box(10, 5, 2, 10, 7, 2, BASE_LIGHT)
        brush.box(5, 5, 5, 5, 7, 5, BASE_LIGHT)
        brush.box(10, 5, 5, 10, 7, 5, BASE_LIGHT)
        brush.place(BASE_LIGHT, 6, 6, 2)
        brush.place(BASE_LIGHT, 9, 6, 2)
        brush.place(BASE_LIGHT, 6, 6, 5)
        brush.place(BASE_LIGHT, 9, 6, 5)
        brush.box(5, 4, 3, 6, 4, 4, BASE_LIGHT)
        brush.box(9, 4, 3, 10, 4, 4, BASE_LIGHT)
        brush.place(LAMP, 5, 4, 2)
        brush.place(LAMP, 5, 4, 5)
        brush.place(LAMP, 10, 4, 2)
        brush.place(LAMP, 10, 4, 5)
        lowerDoors(brush, west, east)
        upperDoors(brush, westUp, eastUp)
    }

    /** The doorways of the lower pair, west half on the left and east half on the right. */
    private fun lowerDoors(
        brush: MonumentBrush,
        west: MonumentRoom,
        east: MonumentRoom,
    ) {
        if (opening(west, StructureDirection.SOUTH)) brush.waterBox(3, 1, 0, 4, 2, 0)
        if (opening(west, StructureDirection.NORTH)) brush.waterBox(3, 1, 7, 4, 2, 7)
        if (opening(west, StructureDirection.WEST)) brush.waterBox(0, 1, 3, 0, 2, 4)
        if (opening(east, StructureDirection.SOUTH)) brush.waterBox(11, 1, 0, 12, 2, 0)
        if (opening(east, StructureDirection.NORTH)) brush.waterBox(11, 1, 7, 12, 2, 7)
        if (opening(east, StructureDirection.EAST)) brush.waterBox(15, 1, 3, 15, 2, 4)
    }

    private fun upperDoors(
        brush: MonumentBrush,
        westUp: MonumentRoom,
        eastUp: MonumentRoom,
    ) {
        if (opening(westUp, StructureDirection.SOUTH)) brush.waterBox(3, 5, 0, 4, 6, 0)
        if (opening(westUp, StructureDirection.NORTH)) brush.waterBox(3, 5, 7, 4, 6, 7)
        if (opening(westUp, StructureDirection.WEST)) brush.waterBox(0, 5, 3, 0, 6, 4)
        if (opening(eastUp, StructureDirection.SOUTH)) brush.waterBox(11, 5, 0, 12, 6, 0)
        if (opening(eastUp, StructureDirection.NORTH)) brush.waterBox(11, 5, 7, 12, 6, 7)
        if (opening(eastUp, StructureDirection.EAST)) brush.waterBox(15, 5, 3, 15, 6, 4)
    }

    fun doubleY(
        brush: MonumentBrush,
        lower: MonumentRoom,
    ) {
        if (aboveGround(lower)) brush.defaultFloor(0, 0, opening(lower, StructureDirection.DOWN))
        val above = up(lower) ?: return
        if (up(above) == null) brush.boxOnFillOnly(1, 8, 1, 6, 8, 6, BASE_GRAY)
        brush.box(0, 4, 0, 0, 4, 7, BASE_LIGHT)
        brush.box(7, 4, 0, 7, 4, 7, BASE_LIGHT)
        brush.box(1, 4, 0, 6, 4, 0, BASE_LIGHT)
        brush.box(1, 4, 7, 6, 4, 7, BASE_LIGHT)
        brush.box(2, 4, 1, 2, 4, 2, BASE_LIGHT)
        brush.box(1, 4, 2, 1, 4, 2, BASE_LIGHT)
        brush.box(5, 4, 1, 5, 4, 2, BASE_LIGHT)
        brush.box(6, 4, 2, 6, 4, 2, BASE_LIGHT)
        brush.box(2, 4, 5, 2, 4, 6, BASE_LIGHT)
        brush.box(1, 4, 5, 1, 4, 5, BASE_LIGHT)
        brush.box(5, 4, 5, 5, 4, 6, BASE_LIGHT)
        brush.box(6, 4, 5, 6, 4, 5, BASE_LIGHT)
        // The lower storey draws its four walls, then the upper one draws the same four again.
        var storey = lower
        for (y in 1..5 step 4) {
            wallOrDoor(brush, storey, StructureDirection.SOUTH, y, 0)
            wallOrDoor(brush, storey, StructureDirection.NORTH, y, 7)
            sideWallOrDoor(brush, storey, StructureDirection.WEST, y, 0)
            sideWallOrDoor(brush, storey, StructureDirection.EAST, y, 7)
            storey = above
        }
    }

    private fun wallOrDoor(
        brush: MonumentBrush,
        room: MonumentRoom,
        direction: StructureDirection,
        y: Int,
        z: Int,
    ) {
        if (opening(room, direction)) {
            brush.box(2, y, z, 2, y + 2, z, BASE_LIGHT)
            brush.box(5, y, z, 5, y + 2, z, BASE_LIGHT)
            brush.box(3, y + 2, z, 4, y + 2, z, BASE_LIGHT)
        } else {
            brush.box(0, y, z, 7, y + 2, z, BASE_LIGHT)
            brush.box(0, y + 1, z, 7, y + 1, z, BASE_GRAY)
        }
    }

    private fun sideWallOrDoor(
        brush: MonumentBrush,
        room: MonumentRoom,
        direction: StructureDirection,
        y: Int,
        x: Int,
    ) {
        if (opening(room, direction)) {
            brush.box(x, y, 2, x, y + 2, 2, BASE_LIGHT)
            brush.box(x, y, 5, x, y + 2, 5, BASE_LIGHT)
            brush.box(x, y + 2, 3, x, y + 2, 4, BASE_LIGHT)
        } else {
            brush.box(x, y, 0, x, y + 2, 7, BASE_LIGHT)
            brush.box(x, y + 1, 0, x, y + 1, 7, BASE_GRAY)
        }
    }

    fun doubleYz(
        brush: MonumentBrush,
        south: MonumentRoom,
    ) {
        val north = south.connections[StructureDirection.NORTH.ordinal] ?: return
        val northUp = up(north) ?: return
        val southUp = up(south) ?: return
        if (aboveGround(south)) {
            brush.defaultFloor(0, 8, opening(north, StructureDirection.DOWN))
            brush.defaultFloor(0, 0, opening(south, StructureDirection.DOWN))
        }
        if (up(southUp) == null) brush.boxOnFillOnly(1, 8, 1, 6, 8, 7, BASE_GRAY)
        if (up(northUp) == null) brush.boxOnFillOnly(1, 8, 8, 6, 8, 14, BASE_GRAY)
        for (y in 1..7) {
            val block = if (y == 2 || y == 6) BASE_GRAY else BASE_LIGHT
            brush.box(0, y, 0, 0, y, 15, block)
            brush.box(7, y, 0, 7, y, 15, block)
            brush.box(1, y, 0, 6, y, 0, block)
            brush.box(1, y, 15, 6, y, 15, block)
        }
        // The pillar between the two halves is dark prismarine banded with two courses of lantern.
        for (y in 1..7) {
            brush.box(3, y, 7, 4, y, 8, if (y == 2 || y == 6) LAMP else BASE_BLACK)
        }
        if (opening(south, StructureDirection.SOUTH)) brush.waterBox(3, 1, 0, 4, 2, 0)
        if (opening(south, StructureDirection.EAST)) brush.waterBox(7, 1, 3, 7, 2, 4)
        if (opening(south, StructureDirection.WEST)) brush.waterBox(0, 1, 3, 0, 2, 4)
        if (opening(north, StructureDirection.NORTH)) brush.waterBox(3, 1, 15, 4, 2, 15)
        if (opening(north, StructureDirection.WEST)) brush.waterBox(0, 1, 11, 0, 2, 12)
        if (opening(north, StructureDirection.EAST)) brush.waterBox(7, 1, 11, 7, 2, 12)
        if (opening(southUp, StructureDirection.SOUTH)) brush.waterBox(3, 5, 0, 4, 6, 0)
        if (opening(southUp, StructureDirection.EAST)) {
            brush.waterBox(7, 5, 3, 7, 6, 4)
            brush.box(5, 4, 2, 6, 4, 5, BASE_LIGHT)
            brush.box(6, 1, 2, 6, 3, 2, BASE_LIGHT)
            brush.box(6, 1, 5, 6, 3, 5, BASE_LIGHT)
        }
        if (opening(southUp, StructureDirection.WEST)) {
            brush.waterBox(0, 5, 3, 0, 6, 4)
            brush.box(1, 4, 2, 2, 4, 5, BASE_LIGHT)
            brush.box(1, 1, 2, 1, 3, 2, BASE_LIGHT)
            brush.box(1, 1, 5, 1, 3, 5, BASE_LIGHT)
        }
        if (opening(northUp, StructureDirection.NORTH)) brush.waterBox(3, 5, 15, 4, 6, 15)
        if (opening(northUp, StructureDirection.WEST)) {
            brush.waterBox(0, 5, 11, 0, 6, 12)
            brush.box(1, 4, 10, 2, 4, 13, BASE_LIGHT)
            brush.box(1, 1, 10, 1, 3, 10, BASE_LIGHT)
            brush.box(1, 1, 13, 1, 3, 13, BASE_LIGHT)
        }
        if (opening(northUp, StructureDirection.EAST)) {
            brush.waterBox(7, 5, 11, 7, 6, 12)
            brush.box(5, 4, 10, 6, 4, 13, BASE_LIGHT)
            brush.box(6, 1, 10, 6, 3, 10, BASE_LIGHT)
            brush.box(6, 1, 13, 6, 3, 13, BASE_LIGHT)
        }
    }

    fun doubleZ(
        brush: MonumentBrush,
        south: MonumentRoom,
    ) {
        val north = south.connections[StructureDirection.NORTH.ordinal] ?: return
        if (aboveGround(south)) {
            brush.defaultFloor(0, 8, opening(north, StructureDirection.DOWN))
            brush.defaultFloor(0, 0, opening(south, StructureDirection.DOWN))
        }
        if (up(south) == null) brush.boxOnFillOnly(1, 4, 1, 6, 4, 7, BASE_GRAY)
        if (up(north) == null) brush.boxOnFillOnly(1, 4, 8, 6, 4, 14, BASE_GRAY)
        for ((y, block) in listOf(3 to BASE_LIGHT, 2 to BASE_GRAY, 1 to BASE_LIGHT)) {
            brush.box(0, y, 0, 0, y, 15, block)
            brush.box(7, y, 0, 7, y, 15, block)
            brush.box(1, y, 0, 7, y, 0, block)
            brush.box(1, y, 15, 6, y, 15, block)
        }
        for (z in listOf(1, 13)) {
            brush.box(1, 1, z, 1, 1, z + 1, BASE_LIGHT)
            brush.box(6, 1, z, 6, 1, z + 1, BASE_LIGHT)
            brush.box(1, 3, z, 1, 3, z + 1, BASE_LIGHT)
            brush.box(6, 3, z, 6, 3, z + 1, BASE_LIGHT)
        }
        brush.box(2, 1, 6, 2, 3, 6, BASE_LIGHT)
        brush.box(5, 1, 6, 5, 3, 6, BASE_LIGHT)
        brush.box(2, 1, 9, 2, 3, 9, BASE_LIGHT)
        brush.box(5, 1, 9, 5, 3, 9, BASE_LIGHT)
        brush.box(3, 2, 6, 4, 2, 6, BASE_LIGHT)
        brush.box(3, 2, 9, 4, 2, 9, BASE_LIGHT)
        brush.box(2, 2, 7, 2, 2, 8, BASE_LIGHT)
        brush.box(5, 2, 7, 5, 2, 8, BASE_LIGHT)
        brush.place(LAMP, 2, 2, 5)
        brush.place(LAMP, 5, 2, 5)
        brush.place(LAMP, 2, 2, 10)
        brush.place(LAMP, 5, 2, 10)
        brush.place(BASE_LIGHT, 2, 3, 5)
        brush.place(BASE_LIGHT, 5, 3, 5)
        brush.place(BASE_LIGHT, 2, 3, 10)
        brush.place(BASE_LIGHT, 5, 3, 10)
        if (opening(south, StructureDirection.SOUTH)) brush.waterBox(3, 1, 0, 4, 2, 0)
        if (opening(south, StructureDirection.EAST)) brush.waterBox(7, 1, 3, 7, 2, 4)
        if (opening(south, StructureDirection.WEST)) brush.waterBox(0, 1, 3, 0, 2, 4)
        if (opening(north, StructureDirection.NORTH)) brush.waterBox(3, 1, 15, 4, 2, 15)
        if (opening(north, StructureDirection.WEST)) brush.waterBox(0, 1, 11, 0, 2, 12)
        if (opening(north, StructureDirection.EAST)) brush.waterBox(7, 1, 11, 7, 2, 12)
    }
}
