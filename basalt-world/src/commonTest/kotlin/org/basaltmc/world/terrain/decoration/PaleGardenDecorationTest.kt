package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class PaleGardenDecorationTest {
    @Test
    fun flowerPatchPlacesClosedEyeblossoms() {
        var total = 0
        for (seed in 0L until 64L) {
            val chunk = flatGrassChunk()
            PaleGardenDecoration.FLOWER_PATCH.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            total += countSurface(chunk)
        }
        assertTrue(total > 0)
    }

    @Test
    fun rareFlowerPlacesForASelectedSeed() {
        val seed =
            (0L..10_000L).first { candidate ->
                WorldgenRandom(XoroshiroRandom(candidate)).nextFloat() < 1.0f / 32.0f
            }
        val chunk = flatGrassChunk()
        PaleGardenDecoration.RARE_FLOWER.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
        assertTrue(countSurface(chunk) > 0)
    }

    private fun flatGrassChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
    }

    private fun countSurface(chunk: Chunk): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) if (chunk.getBlockStateId(x, 63, z) == closedEyeblossomStateId) result++
        }
        return result
    }

    private val closedEyeblossomStateId =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla("closed_eyeblossom"))?.stateId
            ?: error("unknown block 'closed_eyeblossom'")
}
