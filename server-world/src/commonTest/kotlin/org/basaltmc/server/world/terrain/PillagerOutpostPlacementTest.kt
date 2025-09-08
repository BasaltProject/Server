package org.basaltmc.server.world.terrain

import org.basaltmc.server.world.structure.StructurePlacementGrid
import kotlin.test.Test
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The outpost is the only structure here needing all three gates: the grid, a frequency roll only a
 * fifth pass, and a ten-chunk exclusion around every village. The vector is the official server's
 * answer to `locate structure minecraft:pillager_outpost` on seed 12345.
 */
class PillagerOutpostPlacementTest {
    private val seed = 12345L
    private val villages = StructurePlacementGrid(spacing = 34, separation = 8, salt = 10387312)

    private fun grid(exclusion: Boolean = true) =
        StructurePlacementGrid(
            spacing = 32,
            separation = 8,
            salt = 165745296,
            frequency = 0.2f,
            frequencyReduction = StructurePlacementGrid.FrequencyReduction.LEGACY_TYPE_1,
            exclusionZone =
                if (exclusion) StructurePlacementGrid.ExclusionZone(chunkCount = 10) { villages } else null,
        )

    @Test
    fun `the chunk the official server reports passes every gate`() {
        // The jar answered [272, ~, -960], the minimum corner of chunk (17, -60).
        assertTrue(grid().isStructureChunk(seed, 17, -60))
    }

    @Test
    fun `the frequency roll rejects most candidates`() {
        // Without it every cell candidate would hold an outpost, which is the bug this catches.
        val candidates =
            (-8..8).flatMap { cellX ->
                (-8..8).map { cellZ -> grid().candidateChunk(seed, cellX * 32, cellZ * 32) }
            }
        val kept = candidates.count { (x, z) -> grid().isStructureChunk(seed, x, z) }
        assertTrue(kept in 1 until candidates.size / 2, "kept $kept of ${candidates.size}")
    }

    @Test
    fun `a candidate next to a village is excluded`() {
        val candidates =
            (-16..16).flatMap { cellX ->
                (-16..16).map { cellZ -> grid().candidateChunk(seed, cellX * 32, cellZ * 32) }
            }
        val withoutZone = candidates.count { (x, z) -> grid(exclusion = false).isStructureChunk(seed, x, z) }
        val withZone = candidates.count { (x, z) -> grid().isStructureChunk(seed, x, z) }
        assertTrue(withZone < withoutZone, "the zone excluded nothing: $withZone of $withoutZone")
        // And the exclusion really is about villages, not an unconditional refusal.
        assertTrue(withZone > 0, "the zone excluded everything")
    }

    @Test
    fun `a chunk that is not its cell's candidate never holds one`() {
        assertFalse(grid().isStructureChunk(seed, 18, -60))
        assertFalse(grid().isStructureChunk(seed, 17, -59))
    }
}
