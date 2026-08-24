package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.BlockFace
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

class OverworldOreDecorationTest {
    @Test
    fun plainsOresMatchOfficialSolidChunkVector() {
        val chunk = solidStoneChunk()

        OverworldOreDecoration.decorate(chunk, 1234L, plainsEnvironment) { _, y, _ ->
            if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) Blocks.STONE.stateId else Blocks.AIR.stateId
        }

        val (hash, counts) = categoryVector(chunk)
        assertContentEquals(
            intArrayOf(
                90702,
                640,
                1305,
                795,
                1370,
                748,
                938,
                977,
                0,
                575,
                0,
                29,
                0,
                48,
                0,
                20,
                6,
                28,
                1,
                122,
                0,
                0,
                0,
                0,
            ),
            counts,
        )
        assertEquals(-8763238117516798733L, hash)
    }

    @Test
    fun pairedTargetsMatchOfficialSolidDeepslateVector() {
        val deepslate = stateId("deepslate")
        val chunk = solidChunk(deepslate)

        OverworldOreDecoration.decorate(chunk, 1234L, plainsEnvironment) { _, y, _ ->
            if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) deepslate else Blocks.AIR.stateId
        }

        val (hash, counts) = categoryVector(chunk, "deepslate")
        assertContentEquals(
            intArrayOf(
                90702,
                640,
                1305,
                795,
                1370,
                748,
                938,
                60,
                917,
                11,
                564,
                5,
                24,
                0,
                48,
                0,
                26,
                10,
                19,
                7,
                115,
                0,
                0,
                0,
            ),
            counts,
        )
        assertEquals(7242757714604742355L, hash)
    }

    @Test
    fun biomeSpecificOresMatchOfficialSolidChunkVectors() {
        val vectors =
            listOf(
                BiomeVector(
                    "meadow",
                    -3186151866697397707L,
                    intArrayOf(
                        90666,
                        640,
                        1305,
                        795,
                        1369,
                        748,
                        938,
                        977,
                        0,
                        575,
                        0,
                        29,
                        0,
                        48,
                        0,
                        20,
                        6,
                        28,
                        1,
                        122,
                        0,
                        0,
                        37,
                        0,
                    ),
                ),
                BiomeVector(
                    "badlands",
                    9146234113104402611L,
                    intArrayOf(
                        90401,
                        640,
                        1305,
                        795,
                        1365,
                        743,
                        938,
                        977,
                        0,
                        575,
                        0,
                        340,
                        0,
                        48,
                        0,
                        20,
                        6,
                        28,
                        1,
                        122,
                        0,
                        0,
                        0,
                        0,
                    ),
                ),
                BiomeVector(
                    "lush_caves",
                    7308623575503349909L,
                    intArrayOf(
                        86911,
                        640,
                        1305,
                        795,
                        1370,
                        674,
                        925,
                        977,
                        0,
                        575,
                        0,
                        29,
                        0,
                        48,
                        0,
                        20,
                        6,
                        28,
                        1,
                        122,
                        0,
                        3878,
                        0,
                        0,
                    ),
                ),
                BiomeVector(
                    "dripstone_caves",
                    -5709392268970678077L,
                    intArrayOf(
                        90418,
                        640,
                        1305,
                        794,
                        1350,
                        743,
                        938,
                        977,
                        0,
                        575,
                        0,
                        29,
                        0,
                        48,
                        0,
                        20,
                        6,
                        28,
                        1,
                        432,
                        0,
                        0,
                        0,
                        0,
                    ),
                ),
            )

        for (vector in vectors) {
            val chunk = solidStoneChunk()
            OverworldOreDecoration.decorate(chunk, 1234L, environment(vector.biome)) { _, y, _ ->
                if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) Blocks.STONE.stateId else Blocks.AIR.stateId
            }
            val (hash, counts) = categoryVector(chunk)
            assertContentEquals(vector.counts, counts, vector.biome)
            assertEquals(vector.hash, hash, vector.biome)
        }
    }

    @Test
    fun underwaterMagmaAndDiskFeaturesMatchOfficialRegionVectors() {
        val vectors =
            listOf(
                LocalFeatureVector(
                    "plains",
                    -796757617744748969L,
                    intArrayOf(13234, 147, 18944, 78, 73, 0, 13, 23, 0, 0),
                ),
                LocalFeatureVector(
                    "swamp",
                    -1908437634749644452L,
                    intArrayOf(13234, 243, 18944, 78, 0, 0, 13, 0, 0, 0),
                ),
                LocalFeatureVector(
                    "mangrove_swamp",
                    -8970619606409398779L,
                    intArrayOf(13234, 73, 18944, 78, 0, 0, 0, 0, 0, 183),
                ),
            )

        for (vector in vectors) {
            val mangrove = vector.biome == "mangrove_swamp"
            val chunk = localFeatureChunk(mangrove)
            OverworldOreDecoration.decorateFeatures(
                chunk,
                1234L,
                environment(vector.biome),
                listOf(26, 29, 30, 31, 32),
                externalSurfaceHeight = { _, _ -> 41 },
                externalBlockState = { _, y, _ -> localFeatureState(y, mangrove) },
            )
            val (hash, counts) = localFeatureVector(chunk)
            assertContentEquals(vector.counts, counts, vector.biome)
            assertEquals(vector.hash, hash, vector.biome)
        }
    }

    @Test
    fun waterDiskPredicateAcceptsBlocksContainingWater() {
        val chunk =
            Chunk(0, 0).also { chunk ->
                for (z in 0 until 16) {
                    for (x in 0 until 16) {
                        chunk.setBlockStateId(x, 40, z, Blocks.DIRT.stateId)
                        chunk.setBlockStateId(x, 41, z, Blocks.SEAGRASS.stateId)
                    }
                }
            }
        OverworldOreDecoration.decorateFeatures(
            chunk,
            1234L,
            plainsEnvironment,
            listOf(31),
            externalSurfaceHeight = { _, _ -> 41 },
            externalBlockState = { _, y, _ ->
                when (y) {
                    40 -> Blocks.DIRT.stateId
                    41 -> Blocks.SEAGRASS.stateId
                    else -> Blocks.AIR.stateId
                }
            },
        )

        assertTrue(
            (0 until 16).any { z -> (0 until 16).any { x -> chunk.getBlockStateId(x, 40, z) == stateId("clay") } },
        )
    }

    @Test
    fun underwaterMagmaUsesTheCoveredOcclusionFace() {
        val bottom = stateId("stone_slab")
        val top = bottom - 2

        assertTrue(OverworldOreDecoration.hasFullMagmaCover(bottom, BlockFace.DOWN))
        assertFalse(OverworldOreDecoration.hasFullMagmaCover(bottom, BlockFace.UP))
        assertFalse(OverworldOreDecoration.hasFullMagmaCover(top, BlockFace.DOWN))
        assertTrue(OverworldOreDecoration.hasFullMagmaCover(top, BlockFace.UP))
    }

    @Test
    fun configuredUnderwaterMagmaScansFlowingWaterAndPlacesTheFloorBody() {
        val chunk = solidChunk(Blocks.STONE.stateId)
        for (x in 7..9) {
            for (z in 7..9) {
                for (y in 41..44) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId + 1)
            }
        }

        val placed =
            OverworldOreDecoration.placeUnderwaterMagmaAt(
                chunk,
                WorldgenRandom(XoroshiroRandom(99L)),
                8,
                43,
                8,
            )

        assertTrue(placed)
        assertEquals(Blocks.WATER.stateId + 1, chunk.getBlockStateId(8, 43, 8))
        assertTrue(
            (7..9).any { z ->
                (7..9).any { x ->
                    (39..40).any { y -> Blocks.byStateId(chunk.getBlockStateId(x, y, z))?.block?.path == "magma_block" }
                }
            },
        )
    }

    @Test
    fun grassDiskUsesTheSolidPredicateAbove() {
        assertEquals(Blocks.GRASS_BLOCK.stateId, OverworldOreDecoration.grassDiskStateForAbove(Blocks.AIR.stateId))
        assertEquals(Blocks.DIRT.stateId, OverworldOreDecoration.grassDiskStateForAbove(stateId("cobweb")))
        assertEquals(Blocks.DIRT.stateId, OverworldOreDecoration.grassDiskStateForAbove(Blocks.WATER.stateId + 1))
    }

    @Test
    fun infestedOreMatchesOfficialUndergroundDecorationVectors() {
        val stone = solidChunk(Blocks.STONE.stateId)
        OverworldOreDecoration.decorateInfested(stone, 1234L, environment("windswept_hills")) { _, y, _ ->
            if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) Blocks.STONE.stateId else Blocks.AIR.stateId
        }
        val (stoneHash, stoneCounts) = infestedVector(stone, "stone")
        assertContentEquals(intArrayOf(98210, 94, 0), stoneCounts)
        assertEquals(3505126642845752643L, stoneHash)

        val deepslate = solidChunk(stateId("deepslate"))
        OverworldOreDecoration.decorateInfested(deepslate, 1234L, environment("windswept_hills")) { _, y, _ ->
            if (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) stateId("deepslate") else Blocks.AIR.stateId
        }
        val (deepslateHash, deepslateCounts) = infestedVector(deepslate, "deepslate")
        assertContentEquals(intArrayOf(98210, 0, 94), deepslateCounts)
        assertEquals(8683730351028714117L, deepslateHash)

        val plains = solidChunk(Blocks.STONE.stateId)
        OverworldOreDecoration.decorateInfested(plains, 1234L, plainsEnvironment)
        assertContentEquals(intArrayOf(98304, 0, 0), infestedVector(plains, "stone").second)
    }

    @Test
    fun sameSeedAndChunkAreDeterministic() {
        val first = solidStoneChunk()
        val second = solidStoneChunk()

        OverworldOreDecoration.decorate(first, 987654321L, plainsEnvironment)
        OverworldOreDecoration.decorate(second, 987654321L, plainsEnvironment)

        assertEquals(categoryVector(first).first, categoryVector(second).first)
        assertContentEquals(categoryVector(first).second, categoryVector(second).second)
    }

    private fun solidStoneChunk(): Chunk = solidChunk(Blocks.STONE.stateId)

    private fun localFeatureChunk(mangrove: Boolean): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        val stateId = localFeatureState(y, mangrove)
                        if (!Blocks.isAir(stateId)) chunk.setBlockStateId(x, y, z, stateId)
                    }
                }
            }
        }

    private fun localFeatureState(
        y: Int,
        mangrove: Boolean,
    ): Int =
        when {
            y == 40 -> if (mangrove) stateId("mud") else Blocks.DIRT.stateId
            y in 41..62 -> Blocks.WATER.stateId
            y < 40 && (y + 60).mod(8) >= 4 -> Blocks.WATER.stateId
            y < 41 -> Blocks.STONE.stateId
            else -> Blocks.AIR.stateId
        }

    private fun solidChunk(stateId: Int): Chunk =
        Chunk(0, 0).also { chunk ->
            for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                for (z in 0 until 16) {
                    for (x in 0 until 16) chunk.setBlockStateId(x, y, z, stateId)
                }
            }
        }

    private fun categoryVector(
        chunk: Chunk,
        baseBlock: String = "stone",
    ): Pair<Long, IntArray> {
        val paths =
            listOf(
                baseBlock,
                "dirt",
                "gravel",
                "granite",
                "diorite",
                "andesite",
                "tuff",
                "coal_ore",
                "deepslate_coal_ore",
                "iron_ore",
                "deepslate_iron_ore",
                "gold_ore",
                "deepslate_gold_ore",
                "redstone_ore",
                "deepslate_redstone_ore",
                "diamond_ore",
                "deepslate_diamond_ore",
                "lapis_ore",
                "deepslate_lapis_ore",
                "copper_ore",
                "deepslate_copper_ore",
                "clay",
                "emerald_ore",
                "deepslate_emerald_ore",
            )
        val categories = paths.map(::stateId).withIndex().associate { it.value to it.index }
        val counts = IntArray(paths.size)
        var hash = 1L
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val category = categories[chunk.getBlockStateId(x, y, z)] ?: -1
                    hash = 31L * hash + category
                    if (category >= 0) counts[category]++
                }
            }
        }
        return hash to counts
    }

    private fun localFeatureVector(chunk: Chunk): Pair<Long, IntArray> {
        val paths =
            listOf("stone", "dirt", "water", "magma_block", "sand", "sandstone", "clay", "gravel", "grass_block", "mud")
        val categories = paths.map(::stateId).withIndex().associate { it.value to it.index }
        val counts = IntArray(paths.size)
        var hash = 1L
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val category = categories[chunk.getBlockStateId(x, y, z)] ?: -1
                    hash = 31L * hash + category
                    if (category >= 0) counts[category]++
                }
            }
        }
        return hash to counts
    }

    private fun infestedVector(
        chunk: Chunk,
        baseBlock: String,
    ): Pair<Long, IntArray> {
        val categories =
            listOf(baseBlock, "infested_stone", "infested_deepslate")
                .map(::stateId)
                .withIndex()
                .associate { it.value to it.index }
        val counts = IntArray(categories.size)
        var hash = 1L
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val category = categories[chunk.getBlockStateId(x, y, z)] ?: -1
                    hash = 31L * hash + category
                    if (category >= 0) counts[category]++
                }
            }
        }
        return hash to counts
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId
            ?: error("missing vanilla block minecraft:$path")

    private val plainsEnvironment = environment("plains")

    private fun environment(biome: String): SurfaceRuleEnvironment =
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
            ): String = "minecraft:$biome"

            override fun isSteep(
                x: Int,
                z: Int,
            ): Boolean = false
        }

    private data class BiomeVector(
        val biome: String,
        val hash: Long,
        val counts: IntArray,
    )

    private data class LocalFeatureVector(
        val biome: String,
        val hash: Long,
        val counts: IntArray,
    )
}
