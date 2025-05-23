package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertTrue

class HugeMushroomFeatureTest {
    @Test
    fun brownHugeMushroomPlacesStemAndCap() {
        val chunk = chunkWithMycelium(0)
        val random = WorldgenRandom(XoroshiroRandom(5L))
        val placed = HugeMushroomFeature.place(chunk, random, 8, 1, 8, HugeMushroomFeature.Kind.BROWN)
        assertTrue(placed, "expected the huge brown mushroom to place successfully")

        var stems = 0
        var caps = 0
        for (x in 0 until 16) {
            for (y in 1..16) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == stemState) stems++
                    if (id == brownCapState) caps++
                }
            }
        }
        // Minimum height 4, and the brown cap is a full 7x7-minus-corners layer.
        assertTrue(stems >= 4, "expected at least 4 mushroom stem blocks, got $stems")
        assertTrue(caps >= 32, "expected the brown cap layer (7x7 minus corners), got $caps")
    }

    @Test
    fun redHugeMushroomPlacesStemAndCap() {
        val chunk = chunkWithMycelium(0)
        val random = WorldgenRandom(XoroshiroRandom(6L))
        val placed = HugeMushroomFeature.place(chunk, random, 8, 1, 8, HugeMushroomFeature.Kind.RED)
        assertTrue(placed, "expected the huge red mushroom to place successfully")

        var stems = 0
        var caps = 0
        for (x in 0 until 16) {
            for (y in 1..16) {
                for (z in 0 until 16) {
                    val id = chunk.getBlockStateId(x, y, z)
                    if (id == stemState) stems++
                    if (id == redCapState) caps++
                }
            }
        }
        assertTrue(stems >= 4, "expected at least 4 mushroom stem blocks, got $stems")
        assertTrue(caps > 0, "expected at least one red mushroom block")
    }

    private fun chunkWithMycelium(groundY: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                chunk.setBlockStateId(x, groundY, z, myceliumState)
            }
        }
        return chunk
    }

    private companion object {
        fun state(name: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

        val myceliumState = state("mycelium")
        val stemState = state("mushroom_stem")
        val brownCapState = state("brown_mushroom_block")
        val redCapState = state("red_mushroom_block")
    }
}
