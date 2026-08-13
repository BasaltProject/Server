package org.basaltmc.server.world.terrain.decoration

import org.basaltmc.server.random.WorldgenRandom
import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.WorldHeight
import org.basaltmc.server.world.terrain.surface.SurfaceRuleEnvironment
import kotlin.test.Test
import kotlin.test.assertContentEquals
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class GlowLichenDecorationTest {
    @Test
    fun matchesOfficialVegetalDecorationVector() {
        val chunk = patternedChunk()

        GlowLichenDecoration.decorate(
            chunk,
            1234L,
            environment("plains"),
            externalBlockState = { x, y, z -> patternState(x, y, z) },
        )

        val vector = vector(chunk)
        assertContentEquals(intArrayOf(26520, 13416, 53493, 4864, 11), vector.counts)
        assertEquals(9199558829214459799L, vector.categoryHash)
        assertEquals(-6221904531837987764L, vector.rawStateHash)
    }

    @Test
    fun rejectsTheVoidBiome() {
        val chunk = patternedChunk()

        GlowLichenDecoration.decorate(
            chunk,
            1234L,
            environment("the_void"),
            externalBlockState = { x, y, z -> patternState(x, y, z) },
        )

        assertContentEquals(intArrayOf(26520, 13416, 53504, 4864, 0), vector(chunk).counts)
    }

    @Test
    fun placesLichenOnUndergroundStoneFaces() {
        var placed = 0
        for (seed in 0L until 8L) {
            val chunk = layeredCave(water = false)
            GlowLichenDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            placed += countLichen(chunk)
        }
        assertTrue(placed > 0)
    }

    @Test
    fun lichenPlacedInWaterKeepsAFluidState() {
        var waterlogged = 0
        for (seed in 0L until 8L) {
            val chunk = layeredCave(water = true)
            GlowLichenDecoration.decorate(chunk, WorldgenRandom(XoroshiroRandom(seed)))
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..50) {
                        val stateId = chunk.getBlockStateId(x, y, z)
                        if (Blocks.isGlowLichenState(stateId) && Blocks.hasWaterFluid(stateId)) waterlogged++
                    }
                }
            }
        }
        assertTrue(waterlogged > 0)
    }

    private fun layeredCave(water: Boolean): Chunk {
        val chunk = Chunk(0, 0)
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..62) {
                    val hollowLayer = y <= 50 && (y and 1) != 0
                    chunk.setBlockStateId(
                        x,
                        y,
                        z,
                        if (hollowLayer) {
                            if (water) {
                                Blocks.WATER.stateId
                            } else {
                                Blocks.AIR.stateId
                            }
                        } else {
                            Blocks.STONE.stateId
                        },
                    )
                }
            }
        }
        return chunk
    }

    private fun countLichen(chunk: Chunk): Int {
        var result = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in WorldHeight.MIN_Y..50) if (Blocks.isGlowLichenState(chunk.getBlockStateId(x, y, z))) result++
            }
        }
        return result
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

    private fun vector(chunk: Chunk): Vector {
        val paths = listOf("stone", "granite", "air", "water", "glow_lichen")
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
        Blocks.REGISTRY
            .byIdentifier(
                org.basaltmc.wire.common.Identifier
                    .vanilla(path),
            )?.stateId
            ?: error("unknown block '$path'")

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
}
