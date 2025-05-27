package org.basaltmc.world.terrain.decoration

import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.registry.Identifier
import org.basaltmc.world.Chunk
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class BambooVegetationDecorationTest {
    @Test
    fun denseSelectorPlacesTreesBushesOrJungleGrass() {
        val chunk = flatChunk(Blocks.GRASS_BLOCK.stateId)
        BambooVegetationDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(73L)))
        assertTrue(count(chunk, generatedStates) > 0)
    }

    @Test
    fun jungleGrassPatchRejectsPodzolSubstrate() {
        val chunk = flatChunk(Blocks.PODZOL.stateId)
        JungleGrassDecoration.placePatchAt(
            chunk,
            WorldgenRandom(XoroshiroRandom(11L)),
            DecorationPos(8, 63, 8),
        )
        assertEquals(0, count(chunk, grassStates))
    }

    private fun flatChunk(surfaceState: Int): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) chunk.setBlockStateId(x, 62, z, surfaceState)
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
                for (y in 63..100) if (chunk.getBlockStateId(x, y, z) in states) result++
            }
        }
        return result
    }

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private val grassStates = setOf(state("short_grass"), state("fern"))
    private val generatedStates =
        grassStates + setOf(state("oak_log"), state("jungle_log"), state("oak_leaves"), state("jungle_leaves"))
}
