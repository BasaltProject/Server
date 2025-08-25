package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.world.climate.ParameterList
import org.basaltmc.server.world.climate.Sampler
import org.basaltmc.server.world.climate.parameters
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions

/** Block-position biome lookup used by Nether placed-feature filters. */
public interface NetherBiomeLookup {
    public fun blockBiomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache? = null,
    ): Biome
}

/**
 * // parity: `MultiNoiseBiomeSourceParameterList.Preset.NETHER`. Resolves the five Nether biomes
 * from the router's temperature and vegetation noises using vanilla's exact five parameter points,
 * including the non-zero offsets that bias warped forest and basalt deltas.
 */
public class NetherBiomeSource(
    temperature: DensityFunction,
    vegetation: DensityFunction,
    biomeZoomSeed: Long? = null,
) : NetherBiomeLookup {
    private val zoomer = biomeZoomSeed?.let(::BiomeZoomer)
    private val parameters: ParameterList<Biome> =
        ParameterList(
            listOf(
                parameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f) to Biomes.NETHER_WASTES,
                parameters(0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f) to Biomes.SOUL_SAND_VALLEY,
                parameters(0.4f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f) to Biomes.CRIMSON_FOREST,
                parameters(0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.375f) to Biomes.WARPED_FOREST,
                parameters(-0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.175f) to Biomes.BASALT_DELTAS,
            ),
        )
    private val sampler =
        Sampler(
            temperature = temperature,
            humidity = vegetation,
            continentalness = DensityFunctions.zero(),
            erosion = DensityFunctions.zero(),
            depth = DensityFunctions.zero(),
            weirdness = DensityFunctions.zero(),
        )

    /** Direct noise-biome lookup from block coordinates, without BiomeManager zoom. */
    public fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache? = null,
    ): Biome = biomeAtQuart(x shr 2, y shr 2, z shr 2, cache)

    public fun biomeAtQuart(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
        cache: DensityCache? = null,
    ): Biome = parameters.findValue(sampler.sample(quartX, quartY, quartZ, cache))

    /** // parity: BiomeManager.getBiome, used by placed-feature biome filters. */
    override fun blockBiomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache?,
    ): Biome =
        zoomer?.biomeAt(x, y, z) { quartX, quartY, quartZ -> biomeAtQuart(quartX, quartY, quartZ, cache) }
            ?: biomeAt(x, y, z, cache)

    /** One generation worker's raw-quart cache, shared by every Nether decoration in its chunk. */
    internal fun cached(densityCache: DensityCache): NetherBiomeLookup {
        val biomeZoomer = zoomer ?: return this
        val noiseBiomes = HashMap<Long, Biome>()
        return object : NetherBiomeLookup {
            override fun blockBiomeAt(
                x: Int,
                y: Int,
                z: Int,
                cache: DensityCache?,
            ): Biome {
                val activeCache = cache ?: densityCache
                return biomeZoomer.biomeAt(x, y, z) { quartX, quartY, quartZ ->
                    val key =
                        (quartX.toLong() and 0x1FFFFFL) or
                            ((quartY.toLong() and 0xFFFFFL) shl 21) or
                            ((quartZ.toLong() and 0x1FFFFFL) shl 41)
                    noiseBiomes.getOrPut(key) { biomeAtQuart(quartX, quartY, quartZ, activeCache) }
                }
            }
        }
    }
}
