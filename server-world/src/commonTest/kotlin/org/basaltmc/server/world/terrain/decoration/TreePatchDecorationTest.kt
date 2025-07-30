package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class TreePatchDecorationTest {
    @Test
    fun featureIndicesAndBiomeMembershipsMatchOfficialStepNineList() {
        val actual =
            TreePatchDecoration.featureSpecs().associate { spec ->
                spec.name to (spec.index to spec.biomes)
            }

        assertEquals(EXPECTED_FEATURES, actual)
        assertTrue("snowy_taiga" in actual.getValue("trees_taiga").second)
        assertTrue("snowy_taiga" !in actual.getValue("trees_snowy").second)
        assertEquals(5, spec("trees_mangrove").maxWaterDepth)
        assertEquals(2, spec("trees_swamp").maxWaterDepth)
        assertEquals("birch_sapling", spec("trees_birch").survivalSapling)
        assertEquals("oak_sapling", spec("trees_plains").survivalSapling)
    }

    @Test
    fun candidateOriginBiomeWinsOverChunkCentreBiome() {
        val origins = mutableListOf<DecorationPos>()
        TreePatchDecoration.decorateFeature(
            flatChunk(),
            1234L,
            splitEnvironment(splitX = 8, left = "jungle", right = "the_void"),
            "trees_jungle",
        ) { _, position -> origins += position }

        assertTrue(origins.isNotEmpty())
        assertTrue(origins.all { it.x < 8 }, "unexpected origins: $origins")
    }

    @Test
    fun featureRejectsBiomeOutsideItsOfficialMembership() {
        val origins = mutableListOf<DecorationPos>()
        TreePatchDecoration.decorateFeature(
            flatChunk(),
            1234L,
            environment("plains"),
            "trees_jungle",
        ) { _, position -> origins += position }

        assertTrue(origins.isEmpty())
    }

    @Test
    fun outerSaplingSurvivalPredicateRejectsStone() {
        val grassOrigins = mutableListOf<DecorationPos>()
        TreePatchDecoration.decorateFeature(
            flatChunk(),
            1234L,
            environment("birch_forest"),
            "trees_birch",
            placementObserver = { _, position -> grassOrigins += position },
        )
        val stoneOrigins = mutableListOf<DecorationPos>()
        TreePatchDecoration.decorateFeature(
            flatChunk(Blocks.STONE.stateId),
            1234L,
            environment("birch_forest"),
            "trees_birch",
        ) { _, position -> stoneOrigins += position }

        assertTrue(grassOrigins.isNotEmpty())
        assertTrue(stoneOrigins.isEmpty())
    }

    @Test
    fun nestedSelectorSaplingPredicatesRejectStone() {
        val stone = flatChunk(Blocks.STONE.stateId)
        val grass = flatChunk()
        val origin = DecorationPos(8, 41, 8)

        TreeDecoration.placeAt(stone, WorldgenRandom(XoroshiroRandom(0L)), origin, "trees_jungle")
        TreeDecoration.placeAt(grass, WorldgenRandom(XoroshiroRandom(0L)), origin, "trees_jungle")

        assertEquals(0, countTreeBlocks(stone))
        assertTrue(countTreeBlocks(grass) > 0)
    }

    @Test
    fun groveTreesRequireSnowAndScanAbovePowderSnow() {
        val grass = flatChunk()
        val snow = flatChunk(stateId("snow_block"))
        val powder = flatChunk(stateId("snow_block"))
        powder.setBlockStateId(8, 41, 8, stateId("powder_snow"))
        powder.setBlockStateId(8, 42, 8, stateId("powder_snow"))
        val tooDeep = flatChunk(stateId("snow_block"))
        for (y in 41..49) tooDeep.setBlockStateId(8, y, 8, stateId("powder_snow"))
        val origin = DecorationPos(8, 41, 8)

        TreeDecoration.placeAt(grass, WorldgenRandom(XoroshiroRandom(0L)), origin, "trees_grove")
        TreeDecoration.placeAt(snow, WorldgenRandom(XoroshiroRandom(0L)), origin, "trees_grove")
        TreeDecoration.placeAt(powder, WorldgenRandom(XoroshiroRandom(0L)), origin, "trees_grove")
        TreeDecoration.placeAt(tooDeep, WorldgenRandom(XoroshiroRandom(0L)), origin, "trees_grove")

        assertEquals(0, countTreeBlocks(grass))
        assertTrue(countTreeBlocks(snow) > 0)
        assertTrue(treeBlockYs(powder).any { it >= 43 })
        assertEquals(0, countTreeBlocks(tooDeep))
    }

    private fun flatChunk(surfaceStateId: Int = Blocks.GRASS_BLOCK.stateId): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    chunk.setBlockStateId(x, 40, z, surfaceStateId)
                }
            }
        }

    private fun spec(name: String): TreePatchDecoration.FeatureSpec =
        TreePatchDecoration.featureSpecs().single { it.name == name }

    private fun countTreeBlocks(chunk: Chunk): Int = treeBlockYs(chunk).size

    private fun treeBlockYs(chunk: Chunk): List<Int> =
        buildList {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        val path = Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path ?: continue
                        if (path.endsWith("_log") || path.endsWith("_leaves")) add(y)
                    }
                }
            }
        }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY
            .byIdentifier(Identifier.vanilla(path))
            ?.stateId
            ?: error("unknown block '$path'")

    private fun splitEnvironment(
        splitX: Int,
        left: String,
        right: String,
    ): SurfaceRuleEnvironment = environment { x -> if (x < splitX) left else right }

    private fun environment(biome: String): SurfaceRuleEnvironment = environment { biome }

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
                "trees_windswept_savanna" to (1 to setOf("windswept_savanna")),
                "bamboo_vegetation" to (3 to setOf("bamboo_jungle")),
                "trees_sparse_jungle" to (4 to setOf("sparse_jungle")),
                "trees_badlands" to (5 to setOf("wooded_badlands")),
                "trees_jungle" to (7 to setOf("jungle")),
                "trees_savanna" to (9 to setOf("savanna", "savanna_plateau")),
                "pale_garden_vegetation" to (13 to setOf("pale_garden")),
                "dark_forest_vegetation" to (17 to setOf("dark_forest")),
                "trees_flower_forest" to (19 to setOf("flower_forest")),
                "birch_tall" to (23 to setOf("old_growth_birch_forest")),
                "trees_birch" to (24 to setOf("birch_forest")),
                "trees_birch_and_oak_leaf_litter" to (25 to setOf("forest")),
                "trees_meadow" to (37 to setOf("meadow")),
                "trees_windswept_forest" to (39 to setOf("windswept_forest")),
                "trees_old_growth_pine_taiga" to (41 to setOf("old_growth_pine_taiga")),
                "trees_old_growth_spruce_taiga" to (42 to setOf("old_growth_spruce_taiga")),
                "trees_taiga" to (43 to setOf("snowy_taiga", "taiga")),
                "trees_grove" to (44 to setOf("grove")),
                "trees_windswept_hills" to
                    (45 to setOf("windswept_gravelly_hills", "windswept_hills")),
                "trees_snowy" to (46 to setOf("ice_spikes", "snowy_plains")),
                "trees_mangrove" to (47 to setOf("mangrove_swamp")),
                "trees_swamp" to (48 to setOf("swamp")),
                "trees_water" to
                    (
                        50 to
                            setOf(
                                "cold_ocean",
                                "deep_cold_ocean",
                                "deep_frozen_ocean",
                                "deep_lukewarm_ocean",
                                "deep_ocean",
                                "frozen_ocean",
                                "frozen_river",
                                "lukewarm_ocean",
                                "ocean",
                                "river",
                                "warm_ocean",
                            )
                    ),
                "trees_plains" to
                    (52 to setOf("deep_dark", "dripstone_caves", "plains", "sunflower_plains")),
                "trees_cherry" to (56 to setOf("cherry_grove")),
            )
    }
}
