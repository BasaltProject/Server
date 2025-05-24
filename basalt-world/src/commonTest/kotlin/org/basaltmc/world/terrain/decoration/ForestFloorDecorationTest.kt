package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class ForestFloorDecorationTest {
    @Test
    fun birchWildflowersPlaceAcrossSampledSeeds() {
        var placed = 0
        for (seed in 0L until 16L) {
            val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
            ForestFloorDecoration.BIRCH_WILDFLOWERS.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            placed += countSurface(chunk, wildflowersStateId)
        }
        assertTrue(placed > 0)
    }

    @Test
    fun leafLitterRequiresGrassBlockBelow() {
        val grass = flatChunk(Blocks.GRASS_BLOCK.stateId)
        ForestFloorDecoration.LEAF_LITTER.decorate(grass, WorldgenRandom(XoroshiroRandom(7L)))
        assertTrue(countSurface(grass, leafLitterStateId) > 0)

        val dirt = flatChunk(Blocks.DIRT.stateId)
        ForestFloorDecoration.LEAF_LITTER.decorate(dirt, WorldgenRandom(XoroshiroRandom(7L)))
        assertEquals(0, countSurface(dirt, leafLitterStateId))
    }

    private fun flatChunk(groundStateId: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, groundStateId)
        }
        return chunk
    }

    private fun countSurface(
        chunk: Chunk,
        stateId: Int,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (chunk.getBlockStateId(x, 63, z) == stateId) result++
        }
        return result
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val wildflowersStateId = stateId("wildflowers")
    private val leafLitterStateId = stateId("leaf_litter")
}
