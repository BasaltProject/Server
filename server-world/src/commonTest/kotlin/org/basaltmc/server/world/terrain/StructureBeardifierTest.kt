package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.structure.StructureBoundingBox
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The two adaptations only differ in how they measure vertical distance, and nothing else in the
 * generator tells them apart yet: the box kind has no user until the ancient city lands.
 */
class StructureBeardifierTest {
    private val box = StructureBoundingBox(0, 40, 0, 7, 60, 7)
    private val groundLevelDelta = 2
    private val groundY = box.minY + groundLevelDelta

    private fun densityAt(
        adjustment: TerrainAdjustment,
        y: Int,
    ): Double =
        StructureBeardifier(listOf(StructureBeardifier.Rigid(box, groundLevelDelta, adjustment)))
            .compute(FunctionContext(4, y, 4))

    @Test
    fun `both kinds agree at the ground level they share`() {
        assertEquals(
            densityAt(TerrainAdjustment.BEARD_THIN, groundY),
            densityAt(TerrainAdjustment.BEARD_BOX, groundY),
        )
    }

    @Test
    fun `a box keeps carving all the way up its piece and a thin beard stops`() {
        // Above the ground line the contribution is negative: the beard is what empties the rock
        // over a structure. More negative is more carving.
        for (y in groundY + 1..box.maxY) {
            assertTrue(
                densityAt(TerrainAdjustment.BEARD_BOX, y) < densityAt(TerrainAdjustment.BEARD_THIN, y),
                "at y=$y a box should still be carving where a thin beard has faded",
            )
        }
        // Straight above the piece the horizontal distance is zero, so the contribution reduces to
        // a constant: a box carves its whole height by the same amount, floor to ceiling.
        for (y in groundY..box.maxY) {
            assertTrue(
                densityAt(TerrainAdjustment.BEARD_BOX, y) < -0.5,
                "a box should carve at y=$y as hard as anywhere else in the piece",
            )
        }
        assertEquals(0.0, densityAt(TerrainAdjustment.BEARD_THIN, box.maxY), "a thin beard is long gone by the top")
    }

    @Test
    fun `neither kind reaches past the kernel above the piece`() {
        val farAbove = box.maxY + 13
        assertEquals(0.0, densityAt(TerrainAdjustment.BEARD_BOX, farAbove))
        assertEquals(0.0, densityAt(TerrainAdjustment.BEARD_THIN, farAbove))
    }

    @Test
    fun `no adjustment contributes nothing anywhere`() {
        for (y in box.minY..box.maxY) assertEquals(0.0, densityAt(TerrainAdjustment.NONE, y))
    }
}
