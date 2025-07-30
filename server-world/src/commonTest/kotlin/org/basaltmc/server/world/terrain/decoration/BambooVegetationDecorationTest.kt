package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
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
        val snowyPodzol =
            Blocks.stateIdOf(Identifier.vanilla("podzol"), mapOf("snowy" to "true"))
                ?: error("missing snowy podzol")
        for (surface in listOf(Blocks.PODZOL.stateId, snowyPodzol)) {
            val chunk = flatChunk(surface)
            JungleGrassDecoration.placePatchAt(
                chunk,
                WorldgenRandom(XoroshiroRandom(11L)),
                DecorationPos(8, 63, 8),
            )
            assertEquals(0, count(chunk, grassStates))
        }
    }

    @Test
    fun denseSelectorAppliesTheChosenTreesNestedSaplingFilter() {
        val seed =
            (0L..1_000L).first { candidate ->
                WorldgenRandom(XoroshiroRandom(candidate)).nextFloat() < 0.05f
            }
        val stone = flatChunk(Blocks.STONE.stateId)
        val grass = flatChunk(Blocks.GRASS_BLOCK.stateId)
        val origin = DecorationPos(8, 63, 8)

        BambooVegetationDecoration.placeAt(
            stone,
            WorldgenRandom(XoroshiroRandom(seed)),
            origin,
            PlacementContext(stone),
        )
        BambooVegetationDecoration.placeAt(
            grass,
            WorldgenRandom(XoroshiroRandom(seed)),
            origin,
            PlacementContext(grass),
        )

        assertEquals(0, count(stone, generatedStates))
        assertTrue(count(grass, generatedStates) > 0)
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
