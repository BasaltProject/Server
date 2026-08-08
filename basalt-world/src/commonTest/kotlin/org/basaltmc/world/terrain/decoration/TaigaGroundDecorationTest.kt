package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class TaigaGroundDecorationTest {
    @Test
    fun largeFernFeaturePlacesDoublePlants() {
        val seed =
            (0L..10_000L).first { candidate ->
                WorldgenRandom(XoroshiroRandom(candidate)).nextFloat() < 1.0f / 5.0f
            }
        val chunk = flatGrassChunk()
        TaigaGroundDecoration.LARGE_FERN.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))

        var found = false
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, 63, z) == largeFernStateId) {
                    found = true
                    assertEquals(largeFernStateId - 1, chunk.getBlockStateId(x, 64, z))
                }
            }
        }
        assertTrue(found)
    }

    @Test
    fun normalTaigaUsesTheOneToFourGrassFernProvider() {
        var shortGrass = 0
        var ferns = 0
        for (seed in 0L until 16L) {
            val chunk = flatGrassChunk()
            TaigaGroundDecoration.NORMAL.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            shortGrass += countSurface(chunk, shortGrassStateId)
            ferns += countSurface(chunk, fernStateId)
        }
        assertTrue(shortGrass > 0)
        assertTrue(ferns > shortGrass, "expected the weight-4 fern state to dominate")
    }

    @Test
    fun oldGrowthAddsSevenGrassPatchesThenDeadBushes() {
        var grassOrFern = 0
        var deadBushes = 0
        for (seed in 0L until 16L) {
            val chunk = flatGrassChunk()
            TaigaGroundDecoration.OLD_GROWTH.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            grassOrFern += countSurface(chunk, shortGrassStateId) + countSurface(chunk, fernStateId)
            deadBushes += countSurface(chunk, deadBushStateId)
        }
        assertTrue(grassOrFern > 0)
        assertTrue(deadBushes > 0)
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

    private val largeFernStateId = stateId("large_fern")
    private val shortGrassStateId = stateId("short_grass")
    private val fernStateId = stateId("fern")
    private val deadBushStateId = stateId("dead_bush")
}
