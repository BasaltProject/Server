package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
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
            placed += countSurface(chunk, Blocks::isWildflowersState)
        }
        assertTrue(placed > 0)
    }

    @Test
    fun leafLitterRequiresGrassBlockBelow() {
        val grass = flatChunk(Blocks.GRASS_BLOCK.stateId)
        ForestFloorDecoration.LEAF_LITTER.decorate(grass, WorldgenRandom(XoroshiroRandom(7L)))
        assertTrue(countLeafLitter(grass) > 0)

        val dirt = flatChunk(Blocks.DIRT.stateId)
        ForestFloorDecoration.LEAF_LITTER.decorate(dirt, WorldgenRandom(XoroshiroRandom(7L)))
        assertEquals(0, countLeafLitter(dirt))
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
        matches: (Int) -> Boolean,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (matches(chunk.getBlockStateId(x, 63, z))) result++
        }
        return result
    }

    private fun countLeafLitter(chunk: Chunk): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (Blocks.isLeafLitterState(chunk.getBlockStateId(x, 63, z))) result++
        }
        return result
    }
}
