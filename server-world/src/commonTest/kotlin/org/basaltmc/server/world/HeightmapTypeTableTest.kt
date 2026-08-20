package org.basaltmc.server.world

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * The precomputed per-state table [Chunk.HeightmapType.matches] reads has to answer exactly what
 * the predicate chain it replaced answers, for every state id, or heightmaps silently shift.
 */
class HeightmapTypeTableTest {
    @Test
    fun `table agrees with the predicate chain for every block state`() {
        val chunk = Chunk(0, 0)
        for (type in Chunk.HeightmapType.entries) {
            for (stateId in 0 until Blocks.stateCount) {
                assertEquals(
                    expectedMatch(type, stateId),
                    type.matches(stateId),
                    "$type disagreed on state id $stateId",
                )
            }
        }
        // Keeps the unused-chunk warning honest: heightAt is the only caller that matters.
        assertEquals(WorldHeight.MIN_Y, chunk.heightAt(0, 0, Chunk.HeightmapType.WORLD_SURFACE))
        assertFalse(Chunk.HeightmapType.OCEAN_FLOOR.matches(state("oak_sapling")))
        assertFalse(Chunk.HeightmapType.OCEAN_FLOOR.matches(Blocks.WATER.stateId + 1))
        assertTrue(Chunk.HeightmapType.MOTION_BLOCKING.matches(Blocks.WATER.stateId + 1))
    }

    private fun expectedMatch(
        type: Chunk.HeightmapType,
        stateId: Int,
    ): Boolean =
        when (type) {
            Chunk.HeightmapType.WORLD_SURFACE -> !Blocks.isAir(stateId)
            Chunk.HeightmapType.OCEAN_FLOOR -> BlockShapes.blocksMotion(stateId)
            Chunk.HeightmapType.MOTION_BLOCKING -> BlockShapes.blocksMotion(stateId) || Blocks.hasFluid(stateId)
            Chunk.HeightmapType.MOTION_BLOCKING_NO_LEAVES ->
                !Blocks.isLeavesState(stateId) &&
                    (BlockShapes.blocksMotion(stateId) || Blocks.hasFluid(stateId))
        }

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("unknown block '$path'")
}
