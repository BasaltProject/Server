package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class ForestFlowerDecorationTest {
    @Test
    fun normalForestEventuallyPlacesASelectedFlowerPatch() {
        val flowerIds = setOf("lilac", "rose_bush", "peony", "lily_of_the_valley").map(::stateId).toSet()
        var placed = false
        for (seed in 0L until 256L) {
            val chunk = flatGrassChunk()
            ForestFlowerDecoration.NORMAL.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            if (count(chunk, flowerIds) > 0) {
                placed = true
                break
            }
        }
        assertTrue(placed)
    }

    @Test
    fun flowerForestVariantPlacesMoreThanOneOuterPatchForSomeSeed() {
        val flowerIds = setOf("lilac", "rose_bush", "peony", "lily_of_the_valley").map(::stateId).toSet()
        var maximum = 0
        for (seed in 0L until 256L) {
            val chunk = flatGrassChunk()
            ForestFlowerDecoration.FLOWER_FOREST.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            maximum = maxOf(maximum, count(chunk, flowerIds))
        }
        assertTrue(maximum > 16)
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
        stateIds: Set<Int>,
    ): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                if (chunk.getBlockStateId(x, 63, z) in stateIds) result++
                if (chunk.getBlockStateId(x, 64, z) in stateIds) result++
            }
        }
        return result
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")
}
