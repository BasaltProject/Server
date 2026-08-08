package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class MeadowFlowerDecorationTest {
    @Test
    fun dualNoiseFlowersPlaceMeadowStatePalette() {
        val chunk = flatGrassChunk()
        MeadowDecoration.FLOWERS.decorate(chunk, WorldgenRandom(XoroshiroRandom(41L)))
        assertTrue(count(chunk, meadowStates) > 0)
    }

    @Test
    fun meadowWildflowersUseBiomeNoiseDensity() {
        val chunk = flatGrassChunk()
        ForestFloorDecoration.MEADOW_WILDFLOWERS.decorate(chunk, WorldgenRandom(XoroshiroRandom(19L)))
        assertTrue(count(chunk, setOf(stateId("wildflowers"))) > 0)
    }

    private fun flatGrassChunk(): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, Blocks.GRASS_BLOCK.stateId)
        }
        return chunk
    }

    private fun count(
        chunk: Chunk,
        states: Set<Int>,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, 63, z) in states) result++
                if (chunk.getBlockStateId(x, 64, z) in states) result++
            }
        }
        return result
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val meadowStates =
        setOf("tall_grass", "allium", "poppy", "azure_bluet", "dandelion", "cornflower", "oxeye_daisy", "short_grass")
            .map(::stateId)
            .toSet()
}
