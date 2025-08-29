package org.basaltmc.server.world.structure

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The frequency roll is seeded differently in each of vanilla's four named methods, so the wrong one
 * still produces a plausible world with the structure in the wrong places. The vector is the mineshaft
 * set on seed 12345, read back from a real 26.2 server's own `structures.starts`.
 */
class StructureFrequencyTest {
    private val mineshafts =
        StructurePlacementGrid(
            spacing = 1,
            separation = 0,
            salt = 0,
            frequency = 0.004f,
            frequencyReduction = StructurePlacementGrid.FrequencyReduction.LEGACY_TYPE_3,
        )

    @Test
    fun `mineshafts land on the chunks a real server put them on`() {
        val found = mutableListOf<Pair<Int, Int>>()
        for (chunkX in -16 until 16) {
            for (chunkZ in -16 until 16) {
                if (mineshafts.isStructureChunk(SEED, chunkX, chunkZ)) found += chunkX to chunkZ
            }
        }
        assertEquals(VANILLA_STARTS, found, "the mineshaft frequency roll no longer matches 26.2")
    }

    @Test
    fun `a frequency of one keeps every candidate chunk`() {
        val always = StructurePlacementGrid(spacing = 1, separation = 0, salt = 0)
        for (chunkX in -4 until 4) {
            for (chunkZ in -4 until 4) {
                assertTrue(
                    always.isStructureChunk(SEED, chunkX, chunkZ),
                    "spacing 1 with no frequency must accept every chunk, not ($chunkX, $chunkZ)",
                )
            }
        }
    }

    private companion object {
        const val SEED = 12345L

        /** Chunk coordinates, in the iteration order of the scan above. */
        val VANILLA_STARTS =
            listOf(
                -16 to -11,
                -16 to 2,
                -12 to 14,
                -4 to 1,
                -3 to 10,
                -1 to 14,
                1 to -14,
                4 to -1,
                12 to -14,
            )
    }
}
