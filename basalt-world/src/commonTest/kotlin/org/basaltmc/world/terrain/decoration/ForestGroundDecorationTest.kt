package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class ForestGroundDecorationTest {
    @Test
    fun forestGrassPlacesTwoPatches() {
        var placed = 0
        for (seed in 0L until 8L) {
            val chunk = flatGrassChunk()
            ForestGroundDecoration.GRASS.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            placed += countSurface(chunk, shortGrassStateId)
        }
        assertTrue(placed > 0)
    }

    @Test
    fun bushPatchUsesItsRarityFilter() {
        val seed =
            (0L..10_000L).first { candidate ->
                WorldgenRandom(XoroshiroRandom(candidate)).nextFloat() < 1.0f / 4.0f
            }
        val chunk = flatGrassChunk()
        ForestGroundDecoration.BUSH.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
        assertTrue(countSurface(chunk, bushStateId) > 0)
    }

    private fun flatGrassChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
    }

    private fun countSurface(
        chunk: Chunk,
        stateId: Int,
    ): Int {
        var count = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (chunk.getBlockStateId(x, 63, z) == stateId) count++
        }
        return count
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val shortGrassStateId = stateId("short_grass")
    private val bushStateId = stateId("bush")
}
