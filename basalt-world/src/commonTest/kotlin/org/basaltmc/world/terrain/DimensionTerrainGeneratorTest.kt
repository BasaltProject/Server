package org.basaltmc.world.terrain

import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.registry.Biomes
import org.basaltmc.registry.Blocks
import org.basaltmc.world.Chunk
import org.basaltmc.world.ChunkHeight
import org.basaltmc.world.densityfunction.BlendedNoise
import org.basaltmc.world.noise.NormalNoise
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
}
