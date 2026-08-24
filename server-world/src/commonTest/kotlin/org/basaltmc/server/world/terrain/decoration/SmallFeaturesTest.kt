package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class SmallFeaturesTest {
    @Test
    fun pilesRequireASturdyTopFace() {
        val supported = flatChunk(state("stone_slab", mapOf("type" to "top", "waterlogged" to "false")))
        val unsupported = flatChunk(state("stone_slab", mapOf("type" to "bottom", "waterlogged" to "false")))

        SmallFeatures.placeAt(supported, random(), DecorationPos(8, 64, 8), "pile_snow")
        SmallFeatures.placeAt(unsupported, random(), DecorationPos(8, 64, 8), "pile_snow")

        assertTrue(count(supported, "snow") > 0)
        assertEquals(0, count(unsupported, "snow"))
    }

    @Test
    fun sporeBlossomRequiresCentreSupportOnTheCeilingFace() {
        val supported = Chunk(0, 0)
        val unsupported = Chunk(0, 0)
        supported.setBlockStateId(8, 65, 8, state("stone_slab", mapOf("type" to "bottom", "waterlogged" to "false")))
        unsupported.setBlockStateId(8, 65, 8, state("stone_slab", mapOf("type" to "top", "waterlogged" to "false")))

        SmallFeatures.placeAt(supported, random(), DecorationPos(8, 64, 8), "spore_blossom")
        SmallFeatures.placeAt(unsupported, random(), DecorationPos(8, 64, 8), "spore_blossom")

        assertEquals(state("spore_blossom"), supported.getBlockStateId(8, 64, 8))
        assertEquals(Blocks.AIR.stateId, unsupported.getBlockStateId(8, 64, 8))
    }

    @Test
    fun groundPatchesUseThePlacedBlocksSurvivalRule() {
        val chunk = flatChunk(Blocks.STONE.stateId)

        SmallFeatures.placeAt(chunk, random(), DecorationPos(8, 64, 8), "wildflowers_birch_forest")

        assertEquals(0, count(chunk, "wildflowers"))
    }

    private fun flatChunk(surfaceStateId: Int): Chunk =
        Chunk(0, 0).apply {
            for (x in 0 until 16) for (z in 0 until 16) setBlockStateId(x, 63, z, surfaceStateId)
        }

    private fun count(
        chunk: Chunk,
        path: String,
    ): Int {
        val block = Identifier.vanilla(path)
        var total = 0
        for (x in 0 until 16) {
            for (y in 60..70) {
                for (z in 0 until 16) if (Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block == block) total++
            }
        }
        return total
    }

    private fun random(): WorldgenRandom = WorldgenRandom(XoroshiroRandom(9L))

    private fun state(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

    private fun state(
        path: String,
        properties: Map<String, String>,
    ): Int = Blocks.stateIdOf(Identifier.vanilla(path), properties) ?: error("unknown state '$path$properties'")
}
