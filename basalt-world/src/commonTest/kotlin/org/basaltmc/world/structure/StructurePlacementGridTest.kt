package org.basaltmc.world.structure

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

/**
 * Golden vectors captured by running the actual decompiled 26.2 server bytecode
 * (`RandomSpreadStructurePlacement.getPotentialStructureChunk`) through a small JVM harness with
 * the real `villages.json` placement values (spacing 34, separation 8, salt 10387312), not by
 * re-deriving the algorithm from memory.
 */
class StructurePlacementGridTest {
    private val villages = StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312)

    @Test
    fun triangularEndCityGridMatchesOfficial26_2Vectors() {
        val endCities =
            StructurePlacementGrid(
                spacing = 20,
                separation = 11,
                salt = 10387313,
                spreadType = StructurePlacementGrid.SpreadType.TRIANGULAR,
            )

        assertEquals(3 to 4, endCities.candidateChunk(0L, 0, 0))
        assertEquals(27 to 3, endCities.candidateChunk(0L, 20, 0))
        assertEquals(-18 to -17, endCities.candidateChunk(0L, -1, -1))
        assertEquals(-18 to 47, endCities.candidateChunk(42L, -20, 40))
    }

    private val seed = 8392034097L

    @Test
    fun matchesVanillaCandidateChunks() {
        assertEquals(-97 to -89, villages.candidateChunk(seed, -102, -102))
        assertEquals(-81 to 0, villages.candidateChunk(seed, -102, 0))
        assertEquals(7 to 0, villages.candidateChunk(seed, 0, 0))
        assertEquals(43 to 5, villages.candidateChunk(seed, 34, 0))
        assertEquals(70 to 2, villages.candidateChunk(seed, 68, 0))
        assertEquals(106 to 122, villages.candidateChunk(seed, 102, 102))
    }

    @Test
    fun isPlacementChunkAgreesWithCandidateChunk() {
        assertTrue(villages.isPlacementChunk(seed, 7, 0))
        assertEquals(false, villages.isPlacementChunk(seed, 0, 0))
        assertTrue(villages.isPlacementChunk(seed, 43, 5))
    }

    @Test
    fun everyCandidateFallsWithinItsOwnGridCell() {
        for (cellX in -5..5) {
            for (cellZ in -5..5) {
                val (candidateX, candidateZ) = villages.candidateChunk(seed, cellX * 34, cellZ * 34)
                assertTrue(candidateX in cellX * 34 until cellX * 34 + (34 - 8))
                assertTrue(candidateZ in cellZ * 34 until cellZ * 34 + (34 - 8))
            }
        }
    }
}
