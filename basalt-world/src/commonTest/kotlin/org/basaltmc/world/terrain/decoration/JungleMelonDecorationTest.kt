package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class JungleMelonDecorationTest {
    @Test
    fun normalJungleMelonsPlaceOnGrass() {
        val placed = placeAcrossSeeds(JungleMelonDecoration.NORMAL, 64)
        assertTrue(placed > 0)
    }

    @Test
    fun sparseJungleMelonsHonorTheirRarerPipeline() {
        val placed = placeAcrossSeeds(JungleMelonDecoration.SPARSE, 512)
        assertTrue(placed > 0)
    }

    @Test
    fun melonsDoNotPlaceOnDirt() {
        val melon = stateId("melon")
        for (seed in 0L until 64L) {
            val chunk = flatChunk(Blocks.DIRT.stateId)
            JungleMelonDecoration.NORMAL.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            for (x in 0 until 16) {
                for (z in 0 until 16) assertTrue(chunk.getBlockStateId(x, 63, z) != melon)
            }
        }
    }

    private fun placeAcrossSeeds(
        decoration: BiomeVegetalDecoration,
        seeds: Int,
    ): Int {
        val melon = stateId("melon")
        for (seed in 0L until seeds.toLong()) {
            val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
            decoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            var placed = 0
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    if (chunk.getBlockStateId(x, 63, z) == melon) {
                        placed++
                        assertEquals(Blocks.GRASS_BLOCK.stateId, chunk.getBlockStateId(x, 62, z))
                    }
                }
            }
            if (placed > 0) return placed
        }
        return 0
    }

    private fun flatChunk(surface: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, surface)
        }
        return chunk
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}
