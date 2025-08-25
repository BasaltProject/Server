package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.FunctionContext

/**
 * // parity: net.minecraft.world.level.biome.TheEndBiomeSource. The central island is always
 * `the_end`; outside its 64-chunk radius, the End router's erosion value at the containing chunk
 * center selects highlands, midlands, barrens or small islands with the exact vanilla thresholds.
 * Input coordinates are quart positions, matching `BiomeSource.getNoiseBiome`.
 */
public class TheEndBiomeSource(
    private val erosion: DensityFunction,
    biomeZoomSeed: Long? = null,
) {
    private val zoomer = biomeZoomSeed?.let(::BiomeZoomer)

    public fun biomeAtQuart(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
    ): Biome {
        val blockX = quartX shl 2
        val blockY = quartY shl 2
        val blockZ = quartZ shl 2
        val chunkX = blockX shr 4
        val chunkZ = blockZ shr 4
        if (chunkX.toLong() * chunkX + chunkZ.toLong() * chunkZ <= ISLAND_CHUNK_DISTANCE_SQUARED) {
            return Biomes.THE_END
        }

        val centerX = (chunkX * 2 + 1) * 8
        val centerZ = (chunkZ * 2 + 1) * 8
        val heightValue = erosion.compute(FunctionContext(centerX, blockY, centerZ))
        return when {
            heightValue > HIGHLANDS_THRESHOLD -> Biomes.END_HIGHLANDS
            heightValue >= MIDLANDS_THRESHOLD -> Biomes.END_MIDLANDS
            heightValue < SMALL_ISLANDS_THRESHOLD -> Biomes.SMALL_END_ISLANDS
            else -> Biomes.END_BARRENS
        }
    }

    /** // parity: BiomeManager.getBiome, used by placed-feature biome filters. */
    public fun blockBiomeAt(
        x: Int,
        y: Int,
        z: Int,
    ): Biome = zoomer?.biomeAt(x, y, z, ::biomeAtQuart) ?: biomeAtQuart(x shr 2, y shr 2, z shr 2)

    private companion object {
        const val ISLAND_CHUNK_DISTANCE_SQUARED = 4096L
        const val HIGHLANDS_THRESHOLD = 0.25
        const val MIDLANDS_THRESHOLD = -0.0625
        const val SMALL_ISLANDS_THRESHOLD = -0.21875
    }
}
