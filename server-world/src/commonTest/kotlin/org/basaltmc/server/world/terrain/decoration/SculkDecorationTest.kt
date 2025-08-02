package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
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

class SculkDecorationTest {
    @Test
    fun completeDeepDarkSculkBatchMatchesOfficialVector() {
        val chunk = patternedChunk()
        var traceHash = 1L
        var writes = 0

        SculkDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("deep_dark"),
            listOf(0, 1),
            externalBlockState = { x, y, z -> patternState(x, y, z) },
            blockWriteObserver = { x, y, z, stateId ->
                traceHash = 31L * traceHash + x
                traceHash = 31L * traceHash + y
                traceHash = 31L * traceHash + z
                traceHash = 31L * traceHash + stateId
                writes++
            },
        )

        assertEquals(21198, writes)
        assertEquals(6153838224403352467L, traceHash)
        val vector = vector(chunk, SCULK_PATHS)
        assertContentEquals(intArrayOf(24639, 12466, 52727, 4864, 2831, 6, 757, 13, 1), vector.counts)
        assertEquals(-2737779484111903474L, vector.categoryHash)
        assertEquals(-7982650000746352159L, vector.rawStateHash)
    }

    @Test
    fun sculkPatchMatchesOfficialUndergroundDecorationVector() {
        val chunk = patternedChunk()

        SculkDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("deep_dark"),
            listOf(1),
            externalBlockState = { x, y, z -> patternState(x, y, z) },
        )

        val vector =
            vector(
                chunk,
                SCULK_PATHS,
            )
        assertContentEquals(intArrayOf(25328, 12812, 53010, 4864, 1795, 4, 475, 15, 1), vector.counts)
        assertEquals(3000034094904547922L, vector.categoryHash)
        assertEquals(2977731739447271626L, vector.rawStateHash)
    }

    @Test
    fun sculkVeinMatchesOfficialUndergroundDecorationVector() {
        val chunk = patternedChunk()
        var traceHash = 1L
        var writes = 0

        SculkDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("deep_dark"),
            listOf(0),
            externalBlockState = { x, y, z -> patternState(x, y, z) },
            blockWriteObserver = { x, y, z, stateId ->
                traceHash = 31L * traceHash + x
                traceHash = 31L * traceHash + y
                traceHash = 31L * traceHash + z
                traceHash = 31L * traceHash + stateId
                writes++
            },
        )

        assertEquals(86, writes)
        assertEquals(-7038032844405342506L, traceHash)
        val vector = vector(chunk)
        assertContentEquals(intArrayOf(26520, 13416, 53420, 4864, 84), vector.counts)
        assertEquals(-4240124166562415787L, vector.categoryHash)
        assertEquals(-6323371002483719565L, vector.rawStateHash)
    }

    @Test
    fun sculkVeinRejectsOtherBiomes() {
        val chunk = patternedChunk()

        SculkDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("plains"),
            listOf(0),
            externalBlockState = { x, y, z -> patternState(x, y, z) },
        )

        assertContentEquals(intArrayOf(26520, 13416, 53504, 4864, 0), vector(chunk).counts)
    }

    @Test
    fun sculkPatchRejectsOtherBiomes() {
        val chunk = patternedChunk()

        SculkDecoration.decorateFeatures(
            chunk,
            1234L,
            environment("plains"),
            listOf(1),
            externalBlockState = { x, y, z -> patternState(x, y, z) },
        )

        assertContentEquals(
            intArrayOf(26520, 13416, 53504, 4864, 0, 0, 0, 0, 0),
            vector(
                chunk,
                SCULK_PATHS,
            ).counts,
        )
    }

    @Test
    fun sculkVeinAcceptsAllWaterBlocksButOnlySourceWaterlogs() {
        for ((waterLevel, expectedWaterlogged) in listOf(0 to "true", 1 to "false")) {
            val chunk = Chunk(0, 0)
            chunk.setBlockStateId(8, 49, 8, Blocks.STONE.stateId)
            chunk.setBlockStateId(8, 50, 8, Blocks.WATER.stateId + waterLevel)

            val placed =
                SculkDecoration.placeAt(
                    chunk,
                    WorldgenRandom(XoroshiroRandom(99L)),
                    DecorationPos(8, 50, 8),
                    "sculk_vein",
                )

            assertTrue(placed)
            val state = Blocks.byStateId(chunk.getBlockStateId(8, 50, 8))
            assertEquals("sculk_vein", state?.block?.path)
            assertEquals(expectedWaterlogged, state?.properties?.get("waterlogged"))
        }
    }

    private fun patternedChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..WorldHeight.MAX_Y) {
                        chunk.setBlockStateId(x, y, z, patternState(x, y, z))
                    }
                }
            }
        }

    private fun patternState(
        x: Int,
        y: Int,
        z: Int,
    ): Int {
        val layer = (y - WorldHeight.MIN_Y).mod(20)
        if (layer in 4..15) return if (layer == 10) Blocks.WATER.stateId else Blocks.AIR.stateId
        return if ((x + z).mod(3) == 0) stateId("granite") else Blocks.STONE.stateId
    }

    private fun vector(
        chunk: Chunk,
        paths: List<String> = listOf("stone", "granite", "air", "water", "sculk_vein"),
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
        val SCULK_PATHS =
            listOf(
                "stone",
                "granite",
                "air",
                "water",
                "sculk",
                "sculk_catalyst",
                "sculk_vein",
                "sculk_sensor",
                "sculk_shrieker",
            )
    }
}
