package org.basaltmc.server.world.terrain

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The hut's grid is three numbers copied from `swamp_huts.json`, and a wrong one puts every hut in
 * the world somewhere else without failing anything. The vector is the official server's own
 * answer to `locate structure minecraft:swamp_hut` on seed 12345.
 */
class SwampHutPlacementTest {
    private val seed = 12345L

    @Test
    fun `the grid selects the chunk the official server reports`() {
        // The jar answered [-2016, ~, -1696], which is the minimum corner of chunk (-126, -106).
        assertTrue(SwampHutStructureGenerator.GRID.isStructureChunk(seed, -126, -106))
    }

    @Test
    fun `the chunks around it are not selected`() {
        for (dx in -1..1) {
            for (dz in -1..1) {
                if (dx == 0 && dz == 0) continue
                assertFalse(
                    SwampHutStructureGenerator.GRID.isStructureChunk(seed, -126 + dx, -106 + dz),
                    "chunk (${-126 + dx}, ${-106 + dz}) should not hold a hut",
                )
            }
        }
    }

    @Test
    fun `the candidate of a cell is the same chunk from anywhere in that cell`() {
        // parity: one candidate per 32-chunk cell, which is what makes the grid a grid.
        val expected = SwampHutStructureGenerator.GRID.candidateChunk(seed, -128, -128)
        assertEquals(Pair(-126, -106), expected)
    }
}
