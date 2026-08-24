package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class VillageFeaturesTest {
    @Test
    fun oakFeaturePoolElementPlacesTheConfiguredOak() {
        val chunk = flatChunk()

        VillageFeatures.place("minecraft:oak", chunk, 8, 64, 8, WorldgenRandom(XoroshiroRandom(3L)))

        val oakLog = state("oak_log")
        assertTrue((64..72).any { y -> chunk.getBlockStateId(8, y, 8) == oakLog })
    }

    @Test
    fun flowerPlainFeaturePoolElementRunsItsCountAndOffsetPipeline() {
        val chunk = flatChunk()

        VillageFeatures.place("minecraft:flower_plain", chunk, 8, 64, 8, WorldgenRandom(XoroshiroRandom(8L)))

        val flowers =
            setOf(
                state("dandelion"),
                state("poppy"),
                state("azure_bluet"),
                state("oxeye_daisy"),
                state("cornflower"),
                state("orange_tulip"),
                state("red_tulip"),
                state("pink_tulip"),
                state("white_tulip"),
            )
        assertTrue((0 until 16).any { x -> (0 until 16).any { z -> chunk.getBlockStateId(x, 64, z) in flowers } })
    }

    @Test
    fun blockPilesRequireASturdyTopFace() {
        val dirtPath = state("dirt_path")
        val random = WorldgenRandom(XoroshiroRandom(1L))
        val topSlab = state("stone_slab", mapOf("type" to "top", "waterlogged" to "false"))
        val bottomSlab = state("stone_slab", mapOf("type" to "bottom", "waterlogged" to "false"))

        assertTrue(VillageFeatures.mayPlacePileOn(topSlab, dirtPath, random))
        assertFalse(VillageFeatures.mayPlacePileOn(bottomSlab, dirtPath, random))
        assertFalse(VillageFeatures.mayPlacePileOn(state("torch"), dirtPath, random))
    }

    @Test
    fun hayPileDrawsEveryPlacedBlocksAxis() {
        val chunk = flatChunk()

        VillageFeatures.place("minecraft:pile_hay", chunk, 8, 64, 8, WorldgenRandom(XoroshiroRandom(5L)))

        val hayAxes =
            listOf("x", "y", "z").map { axis -> state("hay_block", mapOf("axis" to axis)) }.toSet()
        val placedStates =
            buildSet {
                for (x in 0 until 16) {
                    for (y in 64..65) {
                        for (z in 0 until 16) {
                            val stateId = chunk.getBlockStateId(x, y, z)
                            if (stateId in hayAxes) add(stateId)
                        }
                    }
                }
            }
        assertEquals(hayAxes, placedStates)
    }

    private fun flatChunk(): Chunk =
        Chunk(0, 0).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) setBlockStateId(x, 63, z, Blocks.GRASS_BLOCK.stateId)
            }
        }

    private fun state(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

    private fun state(
        name: String,
        properties: Map<String, String>,
    ): Int = Blocks.stateIdOf(Identifier.vanilla(name), properties) ?: error("unknown state '$name$properties'")
}
