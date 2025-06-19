package org.basaltmc.server.world

import org.basaltmc.server.registry.BlockStateDefinitions
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals

class LeafDistancesTest {
    private fun state(
        name: String,
        properties: Map<String, String> = emptyMap(),
    ): Int = checkNotNull(BlockStateDefinitions.stateIdOf(Identifier.vanilla(name), properties))

    private fun distanceAt(
        chunk: Chunk,
        x: Int,
        y: Int,
        z: Int,
    ): Int? = BlockStateDefinitions.propertiesOf(chunk.getBlockStateId(x, y, z))["distance"]?.toIntOrNull()

    /** A canopy on a trunk: one step out per block, exactly as `TreeFeature.updateLeaves` counts it. */
    @Test
    fun leavesCountOutwardsFromTheTrunk() {
        val chunk = Chunk(0, 0)
        val log = state("oak_log", mapOf("axis" to "y"))
        val leaves = state("oak_leaves", mapOf("distance" to "7"))
        for (y in 64..70) chunk.setBlockStateId(8, y, 8, log)
        for (offset in 1..4) {
            chunk.setBlockStateId(8 + offset, 70, 8, leaves)
        }

        LeafDistances.assign(chunk)

        assertEquals(1, distanceAt(chunk, 9, 70, 8))
        assertEquals(2, distanceAt(chunk, 10, 70, 8))
        assertEquals(3, distanceAt(chunk, 11, 70, 8))
        assertEquals(4, distanceAt(chunk, 12, 70, 8))
    }

    /** What generation leaves behind: every leaf at seven, which is the state that decays. */
    @Test
    fun aDefaultCanopyIsStampedInsteadOfLeftAtSeven() {
        val chunk = Chunk(0, 0)
        val leaves = state("oak_leaves", mapOf("distance" to "7"))
        chunk.setBlockStateId(4, 70, 4, state("birch_log", mapOf("axis" to "y")))
        for (x in 3..5) {
            for (z in 3..5) {
                if (x == 4 && z == 4) continue
                chunk.setBlockStateId(x, 71, z, leaves)
            }
        }
        chunk.setBlockStateId(4, 71, 4, leaves)

        val corrected = LeafDistances.assign(chunk)

        assertEquals(9, corrected)
        assertEquals(1, distanceAt(chunk, 4, 71, 4))
        assertEquals(2, distanceAt(chunk, 3, 71, 4))
        assertEquals(3, distanceAt(chunk, 3, 71, 3))
    }

    /** Leaves with no trunk under them keep the distance that kills them. */
    @Test
    fun leavesOnTheirOwnStayAtSeven() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(2, 80, 2, state("oak_leaves", mapOf("distance" to "3")))

        LeafDistances.assign(chunk)

        assertEquals(3, distanceAt(chunk, 2, 80, 2), "no trunk anywhere, so nothing to walk out from")
    }

    /** A leaf that was stored too near its trunk is corrected, not only the ones left at seven. */
    @Test
    fun aWrongStoredDistanceIsRewritten() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(8, 64, 8, state("oak_log", mapOf("axis" to "y")))
        chunk.setBlockStateId(9, 64, 8, state("oak_leaves", mapOf("distance" to "5")))

        assertEquals(1, LeafDistances.assign(chunk))
        assertEquals(1, distanceAt(chunk, 9, 64, 8))
    }
}
