package org.basaltmc.server.world.structure

import org.basaltmc.server.world.structure.MonumentPalette.BASE_BLACK
import org.basaltmc.server.world.structure.MonumentPalette.BASE_GRAY
import org.basaltmc.server.world.structure.MonumentPalette.BASE_LIGHT
import org.basaltmc.server.world.structure.MonumentPalette.DOT
import org.basaltmc.server.world.structure.MonumentPalette.LAMP

/**
 * // parity: MonumentBuilding.postProcess, the monument's own body down to the pillars that carry it
 * to the sea bed; the rooms inside are separate pieces. Coordinates are the piece's own frame, 58 by
 * 23 by 58, which [StructurePieceFill] turns by the monument's orientation.
 */
internal class MonumentShell(
    private val brush: MonumentBrush,
    private val seaLevel: Int,
) {
    fun place() {
        val waterHeight = maxOf(seaLevel, 64) - brush.fill.worldY(0)
        brush.waterBox(0, 0, 0, 58, waterHeight, 58)
        wing(flipped = false, xOffset = 0)
        wing(flipped = true, xOffset = 33)
        entranceArches()
        entranceWall()
        roof()
        lowerWall()
        middleWall()
        upperWall()
        pillars()
        // Five skirts of water stepping outward and upward, which is what keeps the sea bed from
        // pressing against the walls.
        for (i in 0 until 5) {
            brush.waterBox(-1 - i, i * 2, -1 - i, -1 - i, 23, 58 + i)
            brush.waterBox(58 + i, i * 2, -1 - i, 58 + i, 23, 58 + i)
            brush.waterBox(-i, i * 2, -1 - i, 57 + i, 23, -1 - i)
            brush.waterBox(-i, i * 2, 58 + i, 57 + i, 23, 58 + i)
        }
    }

    /** The seven by seven grid of legs, minus the one the entrance stands in. */
    private fun pillars() {
        for (pillarX in 0 until 7) {
            var pillarZ = 0
            while (pillarZ < 7) {
                if (pillarZ == 0 && pillarX == 3) pillarZ = 6
                val bx = pillarX * 9
                val bz = pillarZ * 9
                for (w in 0 until 4) {
                    for (d in 0 until 4) {
                        brush.place(BASE_LIGHT, bx + w, 0, bz + d)
                        brush.fillColumnDown(BASE_LIGHT, bx + w, -1, bz + d)
                    }
                }
                if (pillarX != 0 && pillarX != 6) pillarZ += 6 else pillarZ++
            }
        }
    }

    private fun wing(
        flipped: Boolean,
        xOffset: Int,
    ) {
        if (!brush.chunkIntersects(xOffset, 0, xOffset + 23, 20)) return
        brush.box(xOffset, 0, 0, xOffset + 24, 0, 20, BASE_GRAY)
        brush.waterBox(xOffset, 1, 0, xOffset + 24, 10, 20)
        for (i in 0 until 4) {
            brush.box(xOffset + i, i + 1, i, xOffset + i, i + 1, 20, BASE_LIGHT)
            brush.box(xOffset + i + 7, i + 5, i + 7, xOffset + i + 7, i + 5, 20, BASE_LIGHT)
            brush.box(xOffset + 17 - i, i + 5, i + 7, xOffset + 17 - i, i + 5, 20, BASE_LIGHT)
            brush.box(xOffset + 24 - i, i + 1, i, xOffset + 24 - i, i + 1, 20, BASE_LIGHT)
            brush.box(xOffset + i + 1, i + 1, i, xOffset + 23 - i, i + 1, i, BASE_LIGHT)
            brush.box(xOffset + i + 8, i + 5, i + 7, xOffset + 16 - i, i + 5, i + 7, BASE_LIGHT)
        }
        brush.box(xOffset + 4, 4, 4, xOffset + 6, 4, 20, BASE_GRAY)
        brush.box(xOffset + 7, 4, 4, xOffset + 17, 4, 6, BASE_GRAY)
        brush.box(xOffset + 18, 4, 4, xOffset + 20, 4, 20, BASE_GRAY)
        brush.box(xOffset + 11, 8, 11, xOffset + 13, 8, 20, BASE_GRAY)
        brush.place(DOT, xOffset + 12, 9, 12)
        brush.place(DOT, xOffset + 12, 9, 15)
        brush.place(DOT, xOffset + 12, 9, 18)
        val left = xOffset + if (flipped) 19 else 5
        val right = xOffset + if (flipped) 5 else 19
        for (z in 20 downTo 5 step 3) brush.place(DOT, left, 5, z)
        for (z in 19 downTo 7 step 3) brush.place(DOT, right, 5, z)
        for (i in 0 until 4) {
            val at = if (flipped) xOffset + 24 - (17 - i * 3) else xOffset + 17 - i * 3
            brush.place(DOT, at, 5, 5)
        }
        brush.place(DOT, right, 5, 5)
        brush.box(xOffset + 11, 1, 12, xOffset + 13, 7, 12, BASE_GRAY)
        brush.box(xOffset + 12, 1, 11, xOffset + 12, 7, 13, BASE_GRAY)
    }

    private fun entranceArches() {
        if (!brush.chunkIntersects(22, 5, 35, 17)) return
        brush.waterBox(25, 0, 0, 32, 8, 20)
        for (i in 0 until 4) {
            val z = 5 + i * 4
            brush.box(24, 2, z, 24, 4, z, BASE_LIGHT)
            brush.box(22, 4, z, 23, 4, z, BASE_LIGHT)
            brush.place(BASE_LIGHT, 25, 5, z)
            brush.place(BASE_LIGHT, 26, 6, z)
            brush.place(LAMP, 26, 5, z)
            brush.box(33, 2, z, 33, 4, z, BASE_LIGHT)
            brush.box(34, 4, z, 35, 4, z, BASE_LIGHT)
            brush.place(BASE_LIGHT, 32, 5, z)
            brush.place(BASE_LIGHT, 31, 6, z)
            brush.place(LAMP, 31, 5, z)
            brush.box(27, 6, z, 30, 6, z, BASE_GRAY)
        }
    }

    private fun entranceWall() {
        if (!brush.chunkIntersects(15, 20, 42, 21)) return
        brush.box(15, 0, 21, 42, 0, 21, BASE_GRAY)
        brush.waterBox(26, 1, 21, 31, 3, 21)
        brush.box(21, 12, 21, 36, 12, 21, BASE_GRAY)
        brush.box(17, 11, 21, 40, 11, 21, BASE_GRAY)
        brush.box(16, 10, 21, 41, 10, 21, BASE_GRAY)
        brush.box(15, 7, 21, 42, 9, 21, BASE_GRAY)
        brush.box(16, 6, 21, 41, 6, 21, BASE_GRAY)
        brush.box(17, 5, 21, 40, 5, 21, BASE_GRAY)
        brush.box(21, 4, 21, 36, 4, 21, BASE_GRAY)
        brush.box(22, 3, 21, 26, 3, 21, BASE_GRAY)
        brush.box(31, 3, 21, 35, 3, 21, BASE_GRAY)
        brush.box(23, 2, 21, 25, 2, 21, BASE_GRAY)
        brush.box(32, 2, 21, 34, 2, 21, BASE_GRAY)
        brush.box(28, 4, 20, 29, 4, 21, BASE_LIGHT)
        brush.place(BASE_LIGHT, 27, 3, 21)
        brush.place(BASE_LIGHT, 30, 3, 21)
        brush.place(BASE_LIGHT, 26, 2, 21)
        brush.place(BASE_LIGHT, 31, 2, 21)
        brush.place(BASE_LIGHT, 25, 1, 21)
        brush.place(BASE_LIGHT, 32, 1, 21)
        for (i in 0 until 7) {
            brush.place(BASE_BLACK, 28 - i, 6 + i, 21)
            brush.place(BASE_BLACK, 29 + i, 6 + i, 21)
        }
        for (i in 0 until 4) {
            brush.place(BASE_BLACK, 28 - i, 9 + i, 21)
            brush.place(BASE_BLACK, 29 + i, 9 + i, 21)
        }
        brush.place(BASE_BLACK, 28, 12, 21)
        brush.place(BASE_BLACK, 29, 12, 21)
        for (i in 0 until 3) {
            brush.place(BASE_BLACK, 22 - i * 2, 8, 21)
            brush.place(BASE_BLACK, 22 - i * 2, 9, 21)
            brush.place(BASE_BLACK, 35 + i * 2, 8, 21)
            brush.place(BASE_BLACK, 35 + i * 2, 9, 21)
        }
        brush.waterBox(15, 13, 21, 42, 15, 21)
        brush.waterBox(15, 1, 21, 15, 6, 21)
        brush.waterBox(16, 1, 21, 16, 5, 21)
        brush.waterBox(17, 1, 21, 20, 4, 21)
        brush.waterBox(21, 1, 21, 21, 3, 21)
        brush.waterBox(22, 1, 21, 22, 2, 21)
        brush.waterBox(23, 1, 21, 24, 1, 21)
        brush.waterBox(42, 1, 21, 42, 6, 21)
        brush.waterBox(41, 1, 21, 41, 5, 21)
        brush.waterBox(37, 1, 21, 40, 4, 21)
        brush.waterBox(36, 1, 21, 36, 3, 21)
        brush.waterBox(33, 1, 21, 34, 1, 21)
        brush.waterBox(35, 1, 21, 35, 2, 21)
    }

    private fun roof() {
        if (!brush.chunkIntersects(21, 21, 36, 36)) return
        brush.box(21, 0, 22, 36, 0, 36, BASE_GRAY)
        brush.waterBox(21, 1, 22, 36, 23, 36)
        for (i in 0 until 4) {
            brush.box(21 + i, 13 + i, 21 + i, 36 - i, 13 + i, 21 + i, BASE_LIGHT)
            brush.box(21 + i, 13 + i, 36 - i, 36 - i, 13 + i, 36 - i, BASE_LIGHT)
            brush.box(21 + i, 13 + i, 22 + i, 21 + i, 13 + i, 35 - i, BASE_LIGHT)
            brush.box(36 - i, 13 + i, 22 + i, 36 - i, 13 + i, 35 - i, BASE_LIGHT)
        }
        brush.box(25, 16, 25, 32, 16, 32, BASE_GRAY)
        brush.box(25, 17, 25, 25, 19, 25, BASE_LIGHT)
        brush.box(32, 17, 25, 32, 19, 25, BASE_LIGHT)
        brush.box(25, 17, 32, 25, 19, 32, BASE_LIGHT)
        brush.box(32, 17, 32, 32, 19, 32, BASE_LIGHT)
        brush.place(BASE_LIGHT, 26, 20, 26)
        brush.place(BASE_LIGHT, 27, 21, 27)
        brush.place(LAMP, 27, 20, 27)
        brush.place(BASE_LIGHT, 26, 20, 31)
        brush.place(BASE_LIGHT, 27, 21, 30)
        brush.place(LAMP, 27, 20, 30)
        brush.place(BASE_LIGHT, 31, 20, 31)
        brush.place(BASE_LIGHT, 30, 21, 30)
        brush.place(LAMP, 30, 20, 30)
        brush.place(BASE_LIGHT, 31, 20, 26)
        brush.place(BASE_LIGHT, 30, 21, 27)
        brush.place(LAMP, 30, 20, 27)
        brush.box(28, 21, 27, 29, 21, 27, BASE_GRAY)
        brush.box(27, 21, 28, 27, 21, 29, BASE_GRAY)
        brush.box(28, 21, 30, 29, 21, 30, BASE_GRAY)
        brush.box(30, 21, 28, 30, 21, 29, BASE_GRAY)
    }

    private fun lowerWall() {
        if (brush.chunkIntersects(0, 21, 6, 58)) {
            brush.box(0, 0, 21, 6, 0, 57, BASE_GRAY)
            brush.waterBox(0, 1, 21, 6, 7, 57)
            brush.box(4, 4, 21, 6, 4, 53, BASE_GRAY)
            for (i in 0 until 4) brush.box(i, i + 1, 21, i, i + 1, 57 - i, BASE_LIGHT)
            for (z in 23 until 53 step 3) brush.place(DOT, 5, 5, z)
            brush.place(DOT, 5, 5, 52)
            // The same four courses again: the jar lays them twice and the second pass is a no-op.
            for (i in 0 until 4) brush.box(i, i + 1, 21, i, i + 1, 57 - i, BASE_LIGHT)
            brush.box(4, 1, 52, 6, 3, 52, BASE_GRAY)
            brush.box(5, 1, 51, 5, 3, 53, BASE_GRAY)
        }
        if (brush.chunkIntersects(51, 21, 58, 58)) {
            brush.box(51, 0, 21, 57, 0, 57, BASE_GRAY)
            brush.waterBox(51, 1, 21, 57, 7, 57)
            brush.box(51, 4, 21, 53, 4, 53, BASE_GRAY)
            for (i in 0 until 4) brush.box(57 - i, i + 1, 21, 57 - i, i + 1, 57 - i, BASE_LIGHT)
            for (z in 23 until 53 step 3) brush.place(DOT, 52, 5, z)
            brush.place(DOT, 52, 5, 52)
            brush.box(51, 1, 52, 53, 3, 52, BASE_GRAY)
            brush.box(52, 1, 51, 52, 3, 53, BASE_GRAY)
        }
        if (brush.chunkIntersects(0, 51, 57, 57)) {
            brush.box(7, 0, 51, 50, 0, 57, BASE_GRAY)
            brush.waterBox(7, 1, 51, 50, 10, 57)
            for (i in 0 until 4) brush.box(i + 1, i + 1, 57 - i, 56 - i, i + 1, 57 - i, BASE_LIGHT)
        }
    }

    private fun middleWall() {
        if (brush.chunkIntersects(7, 21, 13, 50)) {
            brush.box(7, 0, 21, 13, 0, 50, BASE_GRAY)
            brush.waterBox(7, 1, 21, 13, 10, 50)
            brush.box(11, 8, 21, 13, 8, 53, BASE_GRAY)
            for (i in 0 until 4) brush.box(i + 7, i + 5, 21, i + 7, i + 5, 54, BASE_LIGHT)
            for (z in 21..45 step 3) brush.place(DOT, 12, 9, z)
        }
        if (brush.chunkIntersects(44, 21, 50, 54)) {
            brush.box(44, 0, 21, 50, 0, 50, BASE_GRAY)
            brush.waterBox(44, 1, 21, 50, 10, 50)
            brush.box(44, 8, 21, 46, 8, 53, BASE_GRAY)
            for (i in 0 until 4) brush.box(50 - i, i + 5, 21, 50 - i, i + 5, 54, BASE_LIGHT)
            for (z in 21..45 step 3) brush.place(DOT, 45, 9, z)
        }
        if (brush.chunkIntersects(8, 44, 49, 54)) {
            brush.box(14, 0, 44, 43, 0, 50, BASE_GRAY)
            brush.waterBox(14, 1, 44, 43, 10, 50)
            for (x in 12..45 step 3) {
                brush.place(DOT, x, 9, 45)
                brush.place(DOT, x, 9, 52)
                if (x == 12 || x == 18 || x == 24 || x == 33 || x == 39 || x == 45) {
                    brush.place(DOT, x, 9, 47)
                    brush.place(DOT, x, 9, 50)
                    brush.place(DOT, x, 10, 45)
                    brush.place(DOT, x, 10, 46)
                    brush.place(DOT, x, 10, 51)
                    brush.place(DOT, x, 10, 52)
                    brush.place(DOT, x, 11, 47)
                    brush.place(DOT, x, 11, 50)
                    brush.place(DOT, x, 12, 48)
                    brush.place(DOT, x, 12, 49)
                }
            }
            for (i in 0 until 3) brush.box(8 + i, 5 + i, 54, 49 - i, 5 + i, 54, BASE_GRAY)
            brush.box(11, 8, 54, 46, 8, 54, BASE_LIGHT)
            brush.box(14, 8, 44, 43, 8, 53, BASE_GRAY)
        }
    }

    private fun upperWall() {
        if (brush.chunkIntersects(14, 21, 20, 43)) {
            brush.box(14, 0, 21, 20, 0, 43, BASE_GRAY)
            brush.waterBox(14, 1, 22, 20, 14, 43)
            brush.box(18, 12, 22, 20, 12, 39, BASE_GRAY)
            brush.box(18, 12, 21, 20, 12, 21, BASE_LIGHT)
            for (i in 0 until 4) brush.box(i + 14, i + 9, 21, i + 14, i + 9, 43 - i, BASE_LIGHT)
            for (z in 23..39 step 3) brush.place(DOT, 19, 13, z)
        }
        if (brush.chunkIntersects(37, 21, 43, 43)) {
            brush.box(37, 0, 21, 43, 0, 43, BASE_GRAY)
            brush.waterBox(37, 1, 22, 43, 14, 43)
            brush.box(37, 12, 22, 39, 12, 39, BASE_GRAY)
            brush.box(37, 12, 21, 39, 12, 21, BASE_LIGHT)
            for (i in 0 until 4) brush.box(43 - i, i + 9, 21, 43 - i, i + 9, 43 - i, BASE_LIGHT)
            for (z in 23..39 step 3) brush.place(DOT, 38, 13, z)
        }
        if (brush.chunkIntersects(15, 37, 42, 43)) {
            brush.box(21, 0, 37, 36, 0, 43, BASE_GRAY)
            brush.waterBox(21, 1, 37, 36, 14, 43)
            brush.box(21, 12, 37, 36, 12, 39, BASE_GRAY)
            for (i in 0 until 4) brush.box(15 + i, i + 9, 43 - i, 42 - i, i + 9, 43 - i, BASE_LIGHT)
            for (x in 21..36 step 3) brush.place(DOT, x, 13, 38)
        }
    }
}
