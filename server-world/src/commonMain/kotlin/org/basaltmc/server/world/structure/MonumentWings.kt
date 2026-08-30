package org.basaltmc.server.world.structure

import org.basaltmc.server.world.structure.MonumentPalette.BASE_BLACK
import org.basaltmc.server.world.structure.MonumentPalette.BASE_GRAY
import org.basaltmc.server.world.structure.MonumentPalette.BASE_LIGHT
import org.basaltmc.server.world.structure.MonumentPalette.LAMP

/**
 * The three pieces outside the lattice: the two wings, an elder guardian each, and the roof
 * penthouse with the third. // parity: OceanMonumentWingRoom and OceanMonumentPenthouse.
 */
internal object MonumentWings {
    /** [design] is the low bit of the single draw both wings share, so the two are never the same. */
    fun wing(
        brush: MonumentBrush,
        design: Int,
    ) {
        if (design == 0) wingHall(brush) else wingGallery(brush)
    }

    /** Design 0: the long hall, its roof stepping down to the entrance and lanterns down both walls. */
    private fun wingHall(brush: MonumentBrush) {
        for (i in 0 until 4) {
            brush.box(10 - i, 3 - i, 20 - i, 12 + i, 3 - i, 20, BASE_LIGHT)
        }
        brush.box(7, 0, 6, 15, 0, 16, BASE_LIGHT)
        brush.box(6, 0, 6, 6, 3, 20, BASE_LIGHT)
        brush.box(16, 0, 6, 16, 3, 20, BASE_LIGHT)
        brush.box(7, 1, 7, 7, 1, 20, BASE_LIGHT)
        brush.box(15, 1, 7, 15, 1, 20, BASE_LIGHT)
        brush.box(7, 1, 6, 9, 3, 6, BASE_LIGHT)
        brush.box(13, 1, 6, 15, 3, 6, BASE_LIGHT)
        brush.box(8, 1, 7, 9, 1, 7, BASE_LIGHT)
        brush.box(13, 1, 7, 14, 1, 7, BASE_LIGHT)
        brush.box(9, 0, 5, 13, 0, 5, BASE_LIGHT)
        brush.box(10, 0, 7, 12, 0, 7, BASE_BLACK)
        brush.box(8, 0, 10, 8, 0, 12, BASE_BLACK)
        brush.box(14, 0, 10, 14, 0, 12, BASE_BLACK)
        for (z in 18 downTo 7 step 3) {
            brush.place(LAMP, 6, 3, z)
            brush.place(LAMP, 16, 3, z)
        }
        brush.place(LAMP, 10, 0, 10)
        brush.place(LAMP, 12, 0, 10)
        brush.place(LAMP, 10, 0, 12)
        brush.place(LAMP, 12, 0, 12)
        brush.place(LAMP, 8, 3, 6)
        brush.place(LAMP, 14, 3, 6)
        for (x in listOf(4, 18)) {
            for (z in listOf(4, 18)) {
                brush.place(BASE_LIGHT, x, 2, z)
                brush.place(LAMP, x, 1, z)
                brush.place(BASE_LIGHT, x, 0, z)
            }
        }
        brush.place(BASE_LIGHT, 9, 7, 20)
        brush.place(BASE_LIGHT, 13, 7, 20)
        brush.box(6, 0, 21, 7, 4, 21, BASE_LIGHT)
        brush.box(15, 0, 21, 16, 4, 21, BASE_LIGHT)
        brush.spawnElder(11, 2, 16)
    }

    /** Design 1: the open gallery, four lantern pillars under a flat ceiling. */
    private fun wingGallery(brush: MonumentBrush) {
        brush.box(9, 3, 18, 13, 3, 20, BASE_LIGHT)
        brush.box(9, 0, 18, 9, 2, 18, BASE_LIGHT)
        brush.box(13, 0, 18, 13, 2, 18, BASE_LIGHT)
        for (x in listOf(9, 13)) {
            brush.place(BASE_LIGHT, x, 6, 20)
            brush.place(LAMP, x, 5, 20)
            brush.place(BASE_LIGHT, x, 4, 20)
        }
        brush.box(7, 3, 7, 15, 3, 14, BASE_LIGHT)
        for (x in listOf(10, 12)) {
            brush.box(x, 0, 10, x, 6, 10, BASE_LIGHT)
            brush.box(x, 0, 12, x, 6, 12, BASE_LIGHT)
            brush.place(LAMP, x, 0, 10)
            brush.place(LAMP, x, 0, 12)
            brush.place(LAMP, x, 4, 10)
            brush.place(LAMP, x, 4, 12)
        }
        for (x in listOf(8, 14)) {
            brush.box(x, 0, 7, x, 2, 7, BASE_LIGHT)
            brush.box(x, 0, 14, x, 2, 14, BASE_LIGHT)
        }
        brush.box(8, 3, 8, 8, 3, 13, BASE_BLACK)
        brush.box(14, 3, 8, 14, 3, 13, BASE_BLACK)
        brush.spawnElder(11, 5, 13)
    }

    /** // parity: OceanMonumentPenthouse. Its floor is one course below the piece's own origin. */
    fun penthouse(brush: MonumentBrush) {
        brush.box(2, -1, 2, 11, -1, 11, BASE_LIGHT)
        brush.box(0, -1, 0, 1, -1, 11, BASE_GRAY)
        brush.box(12, -1, 0, 13, -1, 11, BASE_GRAY)
        brush.box(2, -1, 0, 11, -1, 1, BASE_GRAY)
        brush.box(2, -1, 12, 11, -1, 13, BASE_GRAY)
        brush.box(0, 0, 0, 0, 0, 13, BASE_LIGHT)
        brush.box(13, 0, 0, 13, 0, 13, BASE_LIGHT)
        brush.box(1, 0, 0, 12, 0, 0, BASE_LIGHT)
        brush.box(1, 0, 13, 12, 0, 13, BASE_LIGHT)
        for (i in 2..11 step 3) {
            brush.place(LAMP, 0, 0, i)
            brush.place(LAMP, 13, 0, i)
            brush.place(LAMP, i, 0, 0)
        }
        brush.box(2, 0, 3, 4, 0, 9, BASE_LIGHT)
        brush.box(9, 0, 3, 11, 0, 9, BASE_LIGHT)
        brush.box(4, 0, 9, 9, 0, 11, BASE_LIGHT)
        brush.place(BASE_LIGHT, 5, 0, 8)
        brush.place(BASE_LIGHT, 8, 0, 8)
        brush.place(BASE_LIGHT, 10, 0, 10)
        brush.place(BASE_LIGHT, 3, 0, 10)
        brush.box(3, 0, 3, 3, 0, 7, BASE_BLACK)
        brush.box(10, 0, 3, 10, 0, 7, BASE_BLACK)
        brush.box(6, 0, 10, 7, 0, 10, BASE_BLACK)
        for (x in listOf(3, 10)) {
            for (z in 2..8 step 3) {
                brush.box(x, 0, z, x, 2, z, BASE_LIGHT)
            }
        }
        brush.box(5, 0, 10, 5, 2, 10, BASE_LIGHT)
        brush.box(8, 0, 10, 8, 2, 10, BASE_LIGHT)
        brush.box(6, -1, 7, 7, -1, 8, BASE_BLACK)
        brush.waterBox(6, -1, 3, 7, -1, 4)
        brush.spawnElder(6, 1, 6)
    }
}
