package org.basaltmc.world.terrain.decoration

import org.basaltmc.common.Identifier
import org.basaltmc.random.WorldgenRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import kotlin.test.Test
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

    private fun flatChunk(): Chunk =
        Chunk(0, 0).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) setBlockStateId(x, 63, z, Blocks.GRASS_BLOCK.stateId)
            }
        }

    private fun state(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")
}
