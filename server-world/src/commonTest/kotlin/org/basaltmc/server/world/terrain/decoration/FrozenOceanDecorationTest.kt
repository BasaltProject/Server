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

class FrozenOceanDecorationTest {
    @Test
    fun seededFrozenOceanFeaturesMatchOfficialVector() {
        val chunk = fullOceanChunk()
        val environment = environment("frozen_ocean")

        FrozenOceanDecoration.decorateLocalModifications(chunk, 14L, environment)
        FrozenOceanDecoration.decorateSurfaceStructures(chunk, 14L, environment)

        val (hash, counts) = vector(chunk)
        assertContentEquals(intArrayOf(32236, 373, 0, 0), counts)
        assertEquals(582792948421561685L, hash)
    }

    @Test
    fun icebergBuildsPackedIceAboveAndBelowSeaLevel() {
        val chunk = oceanChunk()
        val region = ChunkFeatureRegion(chunk)

        IcebergFeature.place(
            region,
            WorldgenRandom(XoroshiroRandom(1234L)),
            DecorationPos(8, 0, 8),
            Blocks.PACKED_ICE.stateId,
        )
        region.flush()

        assertTrue(countState(chunk, Blocks.PACKED_ICE.stateId, 63, 100) > 0)
        assertTrue(countState(chunk, Blocks.PACKED_ICE.stateId, 30, 62) > 0)
    }

    @Test
    fun blueIceSpreadsFromWaterAdjacentToPackedIce() {
        val chunk = oceanChunk()
        chunk.setBlockStateId(9, 50, 8, Blocks.PACKED_ICE.stateId)
        val region = ChunkFeatureRegion(chunk)

        val placed = BlueIceFeature.place(region, WorldgenRandom(XoroshiroRandom(99L)), DecorationPos(8, 50, 8))
        region.flush()

        assertTrue(placed)
        assertEquals(Blocks.BLUE_ICE.stateId, chunk.getBlockStateId(8, 50, 8))
        assertTrue(countState(chunk, Blocks.BLUE_ICE.stateId, 30, 62) > 1)
    }

    @Test
    fun icebergReplacesFlowingWaterBelowSeaLevel() {
        val chunk = oceanChunk(Blocks.WATER.stateId + 1)
        val region = ChunkFeatureRegion(chunk)

        IcebergFeature.place(
            region,
            WorldgenRandom(XoroshiroRandom(1234L)),
            DecorationPos(8, 0, 8),
            Blocks.PACKED_ICE.stateId,
        )
        region.flush()

        assertTrue(countState(chunk, Blocks.PACKED_ICE.stateId, 30, 62) > 0)
    }

    @Test
    fun blueIceAcceptsFlowingWaterOrigin() {
        val chunk = oceanChunk()
        chunk.setBlockStateId(8, 50, 8, Blocks.WATER.stateId + 1)
        chunk.setBlockStateId(9, 50, 8, Blocks.PACKED_ICE.stateId)
        val region = ChunkFeatureRegion(chunk)

        val placed = BlueIceFeature.place(region, WorldgenRandom(XoroshiroRandom(99L)), DecorationPos(8, 50, 8))
        region.flush()

        assertTrue(placed)
        assertEquals(Blocks.BLUE_ICE.stateId, chunk.getBlockStateId(8, 50, 8))
    }

    private fun oceanChunk(stateId: Int = Blocks.WATER.stateId): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in 30..62) chunk.setBlockStateId(x, y, z, stateId)
                }
            }
        }

    private fun fullOceanChunk(): Chunk =
        Chunk(0, 0).also { chunk ->
            for (x in 0 until 16) {
                for (z in 0 until 16) {
                    for (y in WorldHeight.MIN_Y..62) chunk.setBlockStateId(x, y, z, Blocks.WATER.stateId)
                }
            }
        }

    private fun vector(chunk: Chunk): Pair<Long, IntArray> {
        val categories =
            mapOf(
                Blocks.WATER.stateId to 0,
                Blocks.PACKED_ICE.stateId to 1,
                Blocks.BLUE_ICE.stateId to 2,
                Blocks.SNOW_BLOCK.stateId to 3,
            )
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
            ): Int = 63

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

    private fun countState(
        chunk: Chunk,
        stateId: Int,
        minY: Int,
        maxY: Int,
    ): Int {
        var count = 0
        for (x in 0 until 16) {
            for (z in 0 until 16) {
                for (y in minY..maxY) if (chunk.getBlockStateId(x, y, z) == stateId) count++
            }
        }
        return count
    }
}
