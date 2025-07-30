package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import org.basaltmc.wire.common.Identifier
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class CaveSpeleothemDecorationTest {
    @Test
    fun dripstoneClusterMatchesOfficialUndergroundDecorationVector() {
        val chunk = patternedChunk(sulfur = false)

        CaveSpeleothemDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("dripstone_caves"),
            listOf(4),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = false) },
        )

        val vector = vector(chunk, POINTED_PATHS)
        assertContentEquals(intArrayOf(20753, 10472, 8504, 51171, 5019, 2385), vector.counts)
        assertEquals(-6701044027370542885L, vector.categoryHash)
        assertEquals(-6301780347711864222L, vector.rawStateHash)
    }

    @Test
    fun sulfurSpikeClusterMatchesOfficialUndergroundDecorationVector() {
        val chunk = patternedChunk(sulfur = true)

        CaveSpeleothemDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("sulfur_caves"),
            listOf(2),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = true) },
        )

        val vector = vector(chunk, SULFUR_PATHS)
        assertContentEquals(intArrayOf(28970, 10966, 52428, 4864, 1076), vector.counts)
        assertEquals(1346425856344338789L, vector.categoryHash)
        assertEquals(3666097771476113385L, vector.rawStateHash)
    }

    @Test
    fun pointedDripstoneMatchesOfficialUndergroundDecorationVector() {
        val chunk = patternedChunk(sulfur = false)

        CaveSpeleothemDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("dripstone_caves"),
            listOf(5),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = false) },
        )

        val vector = vector(chunk, POINTED_PATHS)
        assertContentEquals(intArrayOf(25464, 12879, 1593, 53056, 4864, 448), vector.counts)
        assertEquals(-7199751178217672096L, vector.categoryHash)
        assertEquals(-719541164016620893L, vector.rawStateHash)
    }

    @Test
    fun sulfurSpikeMatchesOfficialUndergroundDecorationVector() {
        val chunk = patternedChunk(sulfur = true)

        CaveSpeleothemDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("sulfur_caves"),
            listOf(3),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = true) },
        )

        val vector = vector(chunk, SULFUR_PATHS)
        assertContentEquals(intArrayOf(26969, 12967, 53176, 4864, 328), vector.counts)
        assertEquals(-9135065926057049902L, vector.categoryHash)
        assertEquals(-2705566044690020828L, vector.rawStateHash)
    }

    @Test
    fun biomeFiltersRejectBothSpeleothemFeatures() {
        val pointed = patternedChunk(sulfur = false)
        CaveSpeleothemDecoration.decorateFeatures(
            pointed,
            1234L,
            environment("plains"),
            listOf(5),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = false) },
        )
        assertContentEquals(intArrayOf(26520, 13416, 0, 53504, 4864, 0), vector(pointed, POINTED_PATHS).counts)

        val sulfur = patternedChunk(sulfur = true)
        CaveSpeleothemDecoration.decorateFeatures(
            sulfur,
            1234L,
            environment("dripstone_caves"),
            listOf(3),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = true) },
        )
        assertContentEquals(intArrayOf(26520, 13416, 53504, 4864, 0), vector(sulfur, SULFUR_PATHS).counts)
    }

    @Test
    fun biomeFiltersRejectBothClusterFeatures() {
        val pointed = patternedChunk(sulfur = false)
        CaveSpeleothemDecoration.decorateFeatures(
            pointed,
            1234L,
            environment("plains"),
            listOf(4),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = false) },
        )
        assertContentEquals(intArrayOf(26520, 13416, 0, 53504, 4864, 0), vector(pointed, POINTED_PATHS).counts)

        val sulfur = patternedChunk(sulfur = true)
        CaveSpeleothemDecoration.decorateFeatures(
            sulfur,
            1234L,
            environment("plains"),
            listOf(2),
            externalBlockState = { x, y, z -> patternState(x, y, z, sulfur = true) },
        )
        assertContentEquals(intArrayOf(26520, 13416, 53504, 4864, 0), vector(sulfur, SULFUR_PATHS).counts)
    }

    @Test
    fun clusterTreatsEveryStateOfTheWaterBlockAsEmptyWater() {
        val chunk = Chunk(0, 0)
        val origin = DecorationPos(8, 50, 8)
        chunk.setBlockStateId(origin.x, origin.y, origin.z, Blocks.WATER.stateId + 1)

        assertTrue(
            CaveSpeleothemDecoration.placeAt(
                chunk,
                XoroshiroRandom(0L),
                origin,
                "dripstone_cluster",
            ),
        )
    }

    private fun patternedChunk(sulfur: Boolean): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, patternState(x, y, z, sulfur))
                    }
                }
            }
        }

    private fun patternState(
        x: Int,
        y: Int,
        z: Int,
        sulfur: Boolean,
    ): Int {
        val layer = (y - WorldHeight.MIN_Y).mod(20)
        if (layer in 4..15) return if (layer == 10) Blocks.WATER.stateId else Blocks.AIR.stateId
        return if (sulfur) {
            if ((x + z).mod(3) == 0) stateId("cinnabar") else stateId("sulfur")
        } else {
            if ((x + z).mod(3) == 0) stateId("granite") else Blocks.STONE.stateId
        }
    }

    private fun vector(
        chunk: Chunk,
        paths: List<String>,
    ): Vector {
        val categories = paths.withIndex().associate { it.value to it.index }
        val counts = IntArray(paths.size)
        var categoryHash = 1L
        var rawStateHash = 1L
        for (z in 0 until 16) {
            for (x in 0 until 16) {
                for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    val category =
                        Blocks
                            .byStateId(stateId)
                            ?.block
                            ?.path
                            ?.let(categories::get) ?: -1
                    categoryHash = 31L * categoryHash + category
                    rawStateHash = 31L * rawStateHash + stateId
                    if (category >= 0) counts[category]++
                }
            }
        }
        return Vector(categoryHash, rawStateHash, counts)
    }

    private fun stateId(path: String): Int =
        Blocks.REGISTRY.byIdentifier(Identifier.vanilla(path))?.stateId ?: error("unknown block '$path'")

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

    private data class Vector(
        val categoryHash: Long,
        val rawStateHash: Long,
        val counts: IntArray,
    )

    private companion object {
        val POINTED_PATHS = listOf("stone", "granite", "dripstone_block", "air", "water", "pointed_dripstone")
        val SULFUR_PATHS = listOf("sulfur", "cinnabar", "air", "water", "sulfur_spike")
    }
}
