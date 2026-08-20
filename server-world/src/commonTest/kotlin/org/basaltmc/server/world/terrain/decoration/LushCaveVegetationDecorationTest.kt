package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.BlockShapes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class LushCaveVegetationDecorationTest {
    @Test
    fun officialIndicesCountsScansAndOffsetsAreUsed() {
        val actual =
            LushCaveVegetationDecoration.featureSpecs().map { spec ->
                FeatureShape(
                    spec.name,
                    spec.index,
                    spec.minCount,
                    spec.maxCount,
                    spec.scan?.direction,
                    spec.scan?.target,
                    spec.yOffset,
                )
            }
        assertEquals(EXPECTED_FEATURES, actual)
    }

    @Test
    fun classicVinesFilterEveryRandomizedOriginBiome() {
        val origins = mutableListOf<DecorationPos>()
        LushCaveVegetationDecoration.decorateFeature(
            Chunk(0, 0),
            1234L,
            environment { x -> if (x < 8) "lush_caves" else "the_void" },
            "classic_vines_cave_feature",
        ) { origins += it }

        assertTrue(origins.isNotEmpty())
        assertTrue(origins.all { it.x < 8 }, "unexpected origins: $origins")
    }

    @Test
    fun caveFeatureRejectsBiomeOutsideLushCaves() {
        val origins = mutableListOf<DecorationPos>()
        LushCaveVegetationDecoration.decorateFeature(
            caveChunk(),
            1234L,
            environment { "dripstone_caves" },
            "spore_blossom",
        ) { origins += it }
        assertTrue(origins.isEmpty())
    }

    @Test
    fun sporeBlossomScanPlacesAgainstTheCeiling() {
        var found = false
        for (seed in 0L..1_000L) {
            val chunk = caveChunk()
            LushCaveVegetationDecoration.decorateFeature(
                chunk,
                seed,
                environment { "lush_caves" },
                "spore_blossom",
            )
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    if (chunk.getBlockStateId(x, CEILING_Y - 1, z) == SPORE_BLOSSOM_STATE_ID) {
                        found = true
                    }
                }
            }
            if (found) break
        }
        assertTrue(found, "expected a sampled scan to place a spore blossom below the ceiling")
    }

    @Test
    fun solidScanUsesBlockStateSolidRatherThanHeightmapMotionBlocking() {
        val oakSapling = stateId("oak_sapling")

        assertFalse(LushCaveVegetationDecoration.Target.SOLID.matches(oakSapling))
        for (stateId in 0 until Blocks.stateCount) {
            assertEquals(
                BlockShapes.isSolidState(stateId),
                LushCaveVegetationDecoration.Target.SOLID.matches(stateId),
                "solid scan disagreed on state id $stateId",
            )
        }
    }

    private fun caveChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    chunk.setBlockStateId(x, FLOOR_Y, z, Blocks.STONE.stateId)
                    chunk.setBlockStateId(x, CEILING_Y, z, Blocks.STONE.stateId)
                }
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

    private data class FeatureShape(
        val name: String,
        val index: Int,
        val minCount: Int,
        val maxCount: Int,
        val scanDirection: Int?,
        val scanTarget: LushCaveVegetationDecoration.Target?,
        val yOffset: Int,
    )

    private companion object {
        const val FLOOR_Y = 30
        const val CEILING_Y = 50
        val SPORE_BLOSSOM_STATE_ID =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla("spore_blossom"))?.stateId
                ?: error("unknown block 'spore_blossom'")

        fun stateId(path: String): Int =
            Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
                ?: error("unknown block '$path'")

        val EXPECTED_FEATURES =
            listOf(
                FeatureShape(
                    "lush_caves_ceiling_vegetation",
                    27,
                    125,
                    125,
                    1,
                    LushCaveVegetationDecoration.Target.SOLID,
                    -1,
                ),
                FeatureShape("cave_vines", 28, 188, 188, 1, LushCaveVegetationDecoration.Target.STURDY_DOWN, -1),
                FeatureShape("lush_caves_clay", 29, 62, 62, -1, LushCaveVegetationDecoration.Target.SOLID, 1),
                FeatureShape("lush_caves_vegetation", 30, 125, 125, -1, LushCaveVegetationDecoration.Target.SOLID, 1),
                FeatureShape("rooted_azalea_tree", 31, 1, 2, 1, LushCaveVegetationDecoration.Target.SOLID, -1),
                FeatureShape("spore_blossom", 32, 25, 25, 1, LushCaveVegetationDecoration.Target.SOLID, -1),
                FeatureShape("classic_vines_cave_feature", 33, 256, 256, null, null, 0),
            )
    }
}
