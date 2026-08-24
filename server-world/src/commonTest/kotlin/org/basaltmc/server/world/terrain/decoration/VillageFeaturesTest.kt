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
    fun everyVillageTreeAppliesItsSaplingSurvivalFilter() {
        for (feature in listOf("acacia", "oak", "spruce", "pine")) {
            val chunk = flatChunk(Blocks.STONE.stateId)

            VillageFeatures.place("minecraft:$feature", chunk, 8, 64, 8, WorldgenRandom(XoroshiroRandom(3L)))

            assertEquals(Blocks.STONE.stateId, chunk.getBlockStateId(8, 63, 8), feature)
            assertTrue((64..80).all { y -> Blocks.isAir(chunk.getBlockStateId(8, y, 8)) }, feature)
        }
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
    fun blockPilesDoNotWrapAcrossTheChunkBorder() {
        val chunk = flatChunk()
        val pileStates = setOf(state("packed_ice"), state("blue_ice"))

        VillageFeatures.place("minecraft:pile_ice", chunk, 15, 64, 8, WorldgenRandom(XoroshiroRandom(2L)))

        var wrapped = 0
        var local = 0
        for (x in 0..15) {
            for (y in 64..65) {
                for (z in 5..11) {
                    if (chunk.getBlockStateId(x, y, z) !in pileStates) continue
                    if (x <= 2) wrapped++
                    if (x >= 12) local++
                }
            }
        }
        assertEquals(0, wrapped)
        assertTrue(local > 0)
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

    @Test
    fun berryPatchMatchesSnowyGrassAndPlacesMatureBushes() {
        val snowyGrass = state("grass_block", mapOf("snowy" to "true"))
        val chunk = flatChunk(snowyGrass)

        VillageFeatures.place("minecraft:patch_berry_bush", chunk, 8, 64, 8, WorldgenRandom(XoroshiroRandom(7L)))

        val mature = state("sweet_berry_bush", mapOf("age" to "3"))
        val allBerryStates = (0..3).map { age -> state("sweet_berry_bush", mapOf("age" to age.toString())) }.toSet()
        val placed =
            buildList {
                for (x in 0 until 16) {
                    for (y in 61..67) {
                        for (z in 0 until 16) {
                            chunk.getBlockStateId(x, y, z).takeIf { it in allBerryStates }?.let(::add)
                        }
                    }
                }
            }
        assertTrue(placed.isNotEmpty())
        assertTrue(placed.all { it == mature })
    }

    @Test
    fun cactusPlacementFilterUsesItsFullSurvivalRuleWithoutWrapping() {
        val chunk = Chunk(0, 0)
        chunk.setBlockStateId(8, 63, 8, state("sand"))
        chunk.setBlockStateId(9, 64, 8, Blocks.STONE.stateId)
        chunk.setBlockStateId(0, 63, 8, state("sand"))
        val waterloggedAbove = Chunk(0, 0)
        waterloggedAbove.setBlockStateId(8, 63, 8, state("sand"))
        waterloggedAbove.setBlockStateId(
            8,
            65,
            8,
            state("stone_slab", mapOf("type" to "bottom", "waterlogged" to "true")),
        )

        assertFalse(CactusFeature.canPlace(chunk, DecorationPos(8, 64, 8)))
        assertFalse(CactusFeature.canPlace(chunk, DecorationPos(16, 64, 8)))
        assertFalse(CactusFeature.canPlace(waterloggedAbove, DecorationPos(8, 64, 8)))
    }

    private fun flatChunk(surfaceStateId: Int = Blocks.GRASS_BLOCK.stateId): Chunk =
        Chunk(0, 0).apply {
            for (x in 0 until 16) {
                for (z in 0 until 16) setBlockStateId(x, 63, z, surfaceStateId)
            }
        }

    private fun state(name: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(name))?.stateId ?: error("unknown block '$name'")

    private fun state(
        name: String,
        properties: Map<String, String>,
    ): Int = Blocks.stateIdOf(Identifier.vanilla(name), properties) ?: error("unknown state '$name$properties'")
}
