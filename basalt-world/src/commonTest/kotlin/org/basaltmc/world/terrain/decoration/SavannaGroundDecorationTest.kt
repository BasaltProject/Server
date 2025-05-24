package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class SavannaGroundDecorationTest {
    @Test
    fun tallGrassPrePassPlacesDoublePlants() {
        val seed =
            (0L..10_000L).first { candidate ->
                WorldgenRandom(XoroshiroRandom(candidate)).nextFloat() < 1.0f / 5.0f
            }
        val chunk = flatGrassChunk()
        SavannaGroundDecoration.TALL_GRASS.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))

        var found = false
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, 63, z) == tallGrassStateId) {
                    found = true
                    assertEquals(tallGrassStateId - 1, chunk.getBlockStateId(x, 64, z))
                }
            }
        }
        assertTrue(found)
    }

    @Test
    fun normalSavannaUsesMoreGrassAttemptsThanWindswept() {
        var normal = 0
        var windswept = 0
        for (seed in 0L until 8L) {
            val normalChunk = flatGrassChunk()
            val windsweptChunk = flatGrassChunk()
            SavannaGroundDecoration.SAVANNA.decorate(normalChunk, WorldgenRandom(XoroshiroRandom(seed)))
            SavannaGroundDecoration.WINDSWEPT.decorate(windsweptChunk, WorldgenRandom(XoroshiroRandom(seed)))
            normal += countSurface(normalChunk, shortGrassStateId)
            windswept += countSurface(windsweptChunk, shortGrassStateId)
        }
        assertTrue(normal > windswept, "expected twenty outer attempts to exceed five")
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
    private val tallGrassStateId = stateId("tall_grass")
}
