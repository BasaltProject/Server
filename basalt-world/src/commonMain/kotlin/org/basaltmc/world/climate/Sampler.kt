package org.basaltmc.world.climate

import org.basaltmc.world.densityfunction.DensityFunction
import org.basaltmc.world.densityfunction.FunctionContext

/**
 * // parity: net.minecraft.world.level.biome.Climate.Sampler. Samples the six climate axes at a
 * block position (given in "quart" coordinates, vanilla's 4-block biome resolution grid) and
 * quantizes the result into a [TargetPoint], ready for [ParameterList.findValue]. `spawnTarget`
 * and vanilla's `findSpawnPosition` are not ported: spawn search needs real sampled terrain,
 * which depends on the full noise router this project does not build yet (the individual
 * [DensityFunction] node types it would be assembled from do exist, see `densityfunction`).
 */
public class Sampler(
    public val temperature: DensityFunction,
    public val humidity: DensityFunction,
    public val continentalness: DensityFunction,
    public val erosion: DensityFunction,
    public val depth: DensityFunction,
    public val weirdness: DensityFunction,
) {
    public fun sample(
        quartX: Int,
        quartY: Int,
        quartZ: Int,
    ): TargetPoint {
        val context = FunctionContext(quartToBlock(quartX), quartToBlock(quartY), quartToBlock(quartZ))
        return target(
            temperature.compute(context).toFloat(),
            humidity.compute(context).toFloat(),
            continentalness.compute(context).toFloat(),
            erosion.compute(context).toFloat(),
            depth.compute(context).toFloat(),
            weirdness.compute(context).toFloat(),
        )
    }

    private companion object {
        // parity: net.minecraft.core.QuartPos.toBlock. A "quart" is 4 blocks, vanilla's biome grid resolution.
        fun quartToBlock(quart: Int): Int = quart shl 2
    }
}
