package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals

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
    fun sameSeedAndChunkAreDeterministic() {
        val first = solidStoneChunk()
        val second = solidStoneChunk()

        OverworldOreDecoration.decorate(first, 987654321L, plainsEnvironment)
        OverworldOreDecoration.decorate(second, 987654321L, plainsEnvironment)

        assertEquals(categoryVector(first).first, categoryVector(second).first)
        assertContentEquals(categoryVector(first).second, categoryVector(second).second)
    }

    private fun solidStoneChunk(): Chunk = solidChunk(Blocks.STONE.stateId)

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
}
