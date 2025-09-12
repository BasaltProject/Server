package org.basaltmc.server.world.structure

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * The stronghold's 128 positions come out of one shared random, so a missing draw does not fail
 * loudly: it slides every later ring somewhere else. These pin the ring geometry with a biome
 * search that never matches, which is the part that has no other check.
 */
class ConcentricRingsPlacementTest {
    private fun barrenPlacement() =
        ConcentricRingsPlacement(
            distance = 32,
            spread = 3,
            count = 128,
            preferredBiomes = setOf("minecraft:plains"),
            noiseBiomeAt = { _, _, _ -> "minecraft:ocean" },
        )

    @Test
    fun `the rings come out where the official numbers say`() {
        val positions = barrenPlacement().ringPositions(12345L)
        assertEquals(128, positions.size)
        // Computed from `generateRingPositions` with java.util.Random, no biome ever matching, so
        // each position is the raw polar one the loop produced.
        val expected =
            listOf(
                -105 to 124,
                -39 to -107,
                114 to 21,
                293 to 0,
                178 to 308,
                -172 to 299,
                -307 to 0,
                -177 to -306,
            )
        assertEquals(expected, positions.take(expected.size))
        assertEquals(-1163 to 868, positions.last())
    }

    @Test
    fun `every stronghold gets its own chunk`() {
        val positions = barrenPlacement().ringPositions(987654321L)
        assertEquals(positions.size, positions.toSet().size, "two strongholds landed in one chunk")

        // The rings grow outwards: the last one is much further out than the first.
        fun radius(p: Pair<Int, Int>) = p.first * p.first + p.second * p.second
        assertTrue(radius(positions.last()) > radius(positions.first()) * 10)
    }
}
