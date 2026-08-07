package org.basaltmc.world.densityfunction

import org.basaltmc.world.noise.NormalNoise

/**
 * // parity: net.minecraft.world.level.levelgen.DensityFunctions.Noise. Samples [noise] at the
 * context position scaled by [xzScale]/[yScale]. Vanilla wraps the noise in a `NoiseHolder`
 * (deferred/lazy resolution against a registry, plus a `null`-noise zero fallback before that
 * resolution happens); this project's [NormalNoise] is constructed eagerly instead, so there is
 * no equivalent unresolved state to model.
 */
public class Noise(
    public val noise: NormalNoise,
    public val xzScale: Double,
    public val yScale: Double,
) : DensityFunction {
    override fun compute(context: FunctionContext): Double =
        noise.getValue(context.blockX * xzScale, context.blockY * yScale, context.blockZ * xzScale)

    override fun minValue(): Double = -maxValue()

    override fun maxValue(): Double = noise.maxValue
}
