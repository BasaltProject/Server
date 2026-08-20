package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class FlowerPatchDecorationTest {
    @Test
    fun featureIndicesAndBiomeMembershipsMatchOfficialStepNineList() {
        val actual = FlowerPatchDecoration.featureSpecs().associate { it.name to (it.index to it.biomes) }
        assertEquals(EXPECTED_FEATURES, actual)
    }

    @Test
    fun eachFlowerOriginUsesItsOwnBiomeInsteadOfTheChunkCentre() {
        val origins = mutableListOf<DecorationPos>()
        FlowerPatchDecoration.decorateFeature(
            flatChunk(),
            1234L,
            environment { x -> if (x < 8) "cherry_grove" else "the_void" },
            "flower_cherry",
        ) { origins += it }

        assertTrue(origins.isNotEmpty())
        assertTrue(origins.all { it.x < 8 }, "unexpected origins: $origins")
    }

    @Test
    fun flowerFeatureRejectsBiomeOutsideItsMembership() {
        val origins = mutableListOf<DecorationPos>()
        FlowerPatchDecoration.decorateFeature(flatChunk(), 1234L, environment { "plains" }, "flower_cherry") {
            origins += it
        }
        assertTrue(origins.isEmpty())
    }

    private fun flatChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) chunk.setBlockStateId(x, 40, z, Blocks.GRASS_BLOCK.stateId)
            }
        }

    private fun environment(biomeAtX: (Int) -> String): SurfaceRuleEnvironment =
        object : SurfaceRuleEnvironment {
            override fun surfaceDepthNoise(
                x: Int,
                z: Int,
            ): Int = 0

            override fun surfaceSecondaryNoise(
                x: Int,
                z: Int,
            ): Double = 0.0

            override fun sample2d(
                noiseId: String,
                x: Int,
                z: Int,
            ): Double = 0.0

            override fun sample3d(
                noiseId: String,
                x: Int,
                y: Int,
                z: Int,
            ): Double = 0.0

            override fun verticalGradientRandom(
                name: String,
                x: Int,
                y: Int,
                z: Int,
            ): Float = 0.0f

            override fun estimatedSurfaceHeight(
                x: Int,
                z: Int,
            ): Int = WorldHeight.MAX_Y + 1

            override fun biomeAt(
                x: Int,
                y: Int,
                z: Int,
            ): String = "minecraft:${biomeAtX(x)}"

            override fun isSteep(
                x: Int,
                z: Int,
            ): Boolean = false
        }

    private companion object {
        val EXPECTED_FEATURES =
            linkedMapOf(
                "flower_warm" to
                    (10 to setOf("bamboo_jungle", "jungle", "savanna", "savanna_plateau", "sparse_jungle")),
                "pale_garden_flowers" to (15 to setOf("pale_garden")),
                "flower_pale_garden" to (16 to setOf("pale_garden")),
                "flower_forest_flowers" to (18 to setOf("flower_forest")),
                "flower_flower_forest" to (20 to setOf("flower_forest")),
                "forest_flowers" to
                    (21 to setOf("birch_forest", "dark_forest", "forest", "old_growth_birch_forest")),
                "wildflowers_birch_forest" to
                    (22 to setOf("birch_forest", "old_growth_birch_forest")),
                "flower_meadow" to (36 to setOf("meadow")),
                "wildflowers_meadow" to (38 to setOf("meadow")),
                "flower_swamp" to (49 to setOf("swamp")),
                "flower_plains" to
                    (53 to setOf("deep_dark", "dripstone_caves", "plains", "sunflower_plains")),
                "flower_cherry" to (55 to setOf("cherry_grove")),
                "flower_default" to
                    (
                        57 to
                            setOf(
                                "beach",
                                "birch_forest",
                                "cold_ocean",
                                "dark_forest",
                                "deep_cold_ocean",
                                "deep_frozen_ocean",
                                "deep_lukewarm_ocean",
                                "deep_ocean",
                                "desert",
                                "forest",
                                "frozen_ocean",
                                "frozen_river",
                                "ice_spikes",
                                "lukewarm_ocean",
                                "ocean",
                                "old_growth_birch_forest",
                                "old_growth_pine_taiga",
                                "old_growth_spruce_taiga",
                                "river",
                                "snowy_beach",
                                "snowy_plains",
                                "snowy_taiga",
                                "stony_shore",
                                "taiga",
                                "warm_ocean",
                                "windswept_forest",
                                "windswept_gravelly_hills",
                                "windswept_hills",
                                "windswept_savanna",
                            )
                    ),
            )
    }
}
