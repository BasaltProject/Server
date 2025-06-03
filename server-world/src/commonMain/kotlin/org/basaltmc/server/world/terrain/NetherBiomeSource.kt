package org.basaltmc.server.world.terrain

import org.basaltmc.server.registry.Biome
import org.basaltmc.server.registry.Biomes
import org.basaltmc.server.world.climate.ParameterList
import org.basaltmc.server.world.climate.Sampler
import org.basaltmc.server.world.climate.parameters
import org.basaltmc.server.world.densityfunction.DensityCache
import org.basaltmc.server.world.densityfunction.DensityFunction
import org.basaltmc.server.world.densityfunction.DensityFunctions

/**
 * // parity: `MultiNoiseBiomeSourceParameterList.Preset.NETHER`. Resolves the five Nether biomes
 * from the router's temperature and vegetation noises using vanilla's exact five parameter points,
 * including the non-zero offsets that bias warped forest and basalt deltas.
 */
public class NetherBiomeSource(
    temperature: DensityFunction,
    vegetation: DensityFunction,
) {
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

    public fun biomeAt(
        x: Int,
        y: Int,
        z: Int,
        cache: DensityCache? = null,
    ): Biome = parameters.findValue(sampler.sample(x shr 2, y shr 2, z shr 2, cache))
}
