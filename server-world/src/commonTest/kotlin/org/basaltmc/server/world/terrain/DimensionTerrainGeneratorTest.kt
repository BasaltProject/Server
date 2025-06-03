package org.basaltmc.server.world.terrain

import org.basaltmc.server.random.XoroshiroRandom
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.registry.Blocks
import org.basaltmc.server.world.Chunk
import org.basaltmc.server.world.ChunkHeight
import org.basaltmc.server.world.densityfunction.BlendedNoise
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext
import org.basaltmc.server.world.noise.NormalNoise
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertTrue

class DimensionTerrainGeneratorTest {
    @Test
    fun netherUsesItsNoiseHeightBlocksFluidsAndBiomePalette() {
        val base3d = BlendedNoise.createUnseeded(0.25, 0.375, 80.0, 60.0, 8.0)
        val router = NoiseRouterData.nether(::syntheticNoise, base3d)
        val biomeSource = NetherBiomeSource(router.temperature, router.vegetation)
        val chunk = DimensionTerrainGenerator.nether(router, biomeSource, 0, 0)

        assertEquals(ChunkHeight.NETHER, chunk.verticalBounds)
        assertTrue(count(chunk, Blocks.NETHERRACK.stateId, 0, 127) > 0)
        assertTrue(count(chunk, Blocks.AIR.stateId, 128, 255) == 16 * 16 * 128)
        val netherBiomeIds =
            setOf(
                Biomes.NETHER_WASTES,
                Biomes.SOUL_SAND_VALLEY,
                Biomes.CRIMSON_FOREST,
                Biomes.WARPED_FOREST,
                Biomes.BASALT_DELTAS,
            ).mapTo(mutableSetOf()) { Biomes.REGISTRY.idOf(it.id) ?: error("unregistered biome ${it.id}") }
        for (section in 0 until chunk.sectionCount) {
            assertTrue(chunk.section(section).biomeIds().all { it in netherBiomeIds })
        }
    }

    @Test
    fun endUsesEightByFourCellsAndCentralEndBiome() {
        val base3d = BlendedNoise.createUnseeded(0.25, 0.25, 80.0, 160.0, 4.0)
        val router = NoiseRouterData.end(base3d, 1234L)
        val chunk = DimensionTerrainGenerator.end(router, TheEndBiomeSource(router.erosion), 0, 0)

        assertEquals(ChunkHeight.END, chunk.verticalBounds)
        assertTrue(count(chunk, Blocks.END_STONE.stateId, 0, 127) > 0)
        assertTrue(count(chunk, Blocks.AIR.stateId, 128, 255) == 16 * 16 * 128)
        val endId = Biomes.REGISTRY.idOf(Biomes.THE_END.id)
        for (section in 0 until chunk.sectionCount) {
            assertTrue(chunk.section(section).biomeIds().all { it == endId })
        }
    }

    @Test
    fun smallEndIslandsMatchOfficialPlacedFeatureVector() {
        val router = EndNoiseRouter(ConstantDensity(-1.0), ConstantDensity(-1.0))
        val biomeSource = TheEndBiomeSource(router.erosion)

        val chunk = DimensionTerrainGenerator.end(router, biomeSource, 65, 0, seed = 27L)

        var count = 0
        var checksum = 0L
        for (y in chunk.verticalBounds.minY..chunk.verticalBounds.maxY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    val stateId = chunk.getBlockStateId(x, y, z)
                    if (stateId != Blocks.END_STONE.stateId) continue
                    count++
                    checksum =
                        checksum * 31 +
                        (chunk.chunkX * 16 + x) * 73428767L +
                        y * 912931L +
                        (chunk.chunkZ * 16 + z) * 438289L +
                        stateId
                }
            }
        }
        assertEquals(461, count)
        assertEquals(1924761240665797445L, checksum)
    }

    @Test
    fun endIslandsRespectTheSmallEndIslandsBiomeFilter() {
        val router = EndNoiseRouter(ConstantDensity(1.0), ConstantDensity(-1.0))
        val chunk = DimensionTerrainGenerator.end(router, TheEndBiomeSource(router.erosion), 65, 0, seed = 27L)

        assertEquals(0, count(chunk, Blocks.END_STONE.stateId, 0, 127))
    }

    private fun syntheticNoise(name: String): NormalNoise =
        NormalNoise.create(
            XoroshiroRandom(name.hashCode().toLong()),
            -5,
            doubleArrayOf(1.0, 1.0, 1.0, 1.0, 1.0),
        )

    private fun count(
        chunk: Chunk,
        stateId: Int,
        minY: Int,
        maxY: Int,
    ): Int {
        var count = 0
        for (y in minY..maxY) {
            for (z in 0 until 16) {
                for (x in 0 until 16) {
                    if (chunk.getBlockStateId(x, y, z) == stateId) count++
                }
            }
        }
        return count
    }

    private class ConstantDensity(
        private val value: Double,
    ) : DensityFunction {
        override fun compute(context: FunctionContext): Double = value

        override fun minValue(): Double = value

        override fun maxValue(): Double = value
    }
}
