package org.basaltmc.world.densityfunction

import org.basaltmc.common.Mth
import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.XoroshiroRandom
import org.basaltmc.world.noise.PerlinNoise

/**
 * // parity: net.minecraft.world.level.levelgen.synth.BlendedNoise. Vanilla's base 3D terrain
 * noise for every dimension (overworld/nether/end all sample one of these, see
 * `NoiseRouterData.BASE_3D_NOISE_*`): three legacy-seeded [PerlinNoise] instances (`minLimitNoise`
 * octaves `-15..0`, `maxLimitNoise` octaves `-15..0`, `mainNoise` octaves `-7..0`) blended by how
 * far `mainNoise` leans toward its own extremes. [createUnseeded] matches vanilla's own
 * `BlendedNoise.createUnseeded`: always seeded with [XoroshiroRandom] seed `0`, since the
 * overworld/nether/end noise router registers this once at bootstrap with a fixed placeholder
 * seed, not the world seed (the real per-world seed only reaches it later, via `withNewRandom`,
 * which this project has not needed yet since nothing assembles a full noise router).
 */
public class BlendedNoise private constructor(
    private val minLimitNoise: PerlinNoise,
    private val maxLimitNoise: PerlinNoise,
    private val mainNoise: PerlinNoise,
    private val xzScale: Double,
    private val yScale: Double,
    private val xzFactor: Double,
    private val yFactor: Double,
    private val smearScaleMultiplier: Double,
) : DensityFunction {
    private val xzMultiplier = XZ_YZ_SCALE * xzScale
    private val yMultiplier = XZ_YZ_SCALE * yScale
    private val cachedMaxValue = minLimitNoise.maxBrokenValue(yMultiplier)

    public constructor(
        random: BasaltRandom,
        xzScale: Double,
        yScale: Double,
        xzFactor: Double,
        yFactor: Double,
        smearScaleMultiplier: Double,
    ) : this(
        PerlinNoise.createLegacyForBlendedNoise(random, (-15..0).toList()),
        PerlinNoise.createLegacyForBlendedNoise(random, (-15..0).toList()),
        PerlinNoise.createLegacyForBlendedNoise(random, (-7..0).toList()),
        xzScale,
        yScale,
        xzFactor,
        yFactor,
        smearScaleMultiplier,
    )

    override fun compute(context: FunctionContext): Double {
        val limitX = context.blockX * xzMultiplier
        val limitY = context.blockY * yMultiplier
        val limitZ = context.blockZ * xzMultiplier
        val mainX = limitX / xzFactor
        val mainY = limitY / yFactor
        val mainZ = limitZ / xzFactor
        val limitSmear = yMultiplier * smearScaleMultiplier
        val mainSmear = limitSmear / yFactor

        var mainNoiseValue = 0.0
        var pow = 1.0
        for (i in 0 until 8) {
            val noise = mainNoise.getOctaveNoise(i)
            if (noise != null) {
                mainNoiseValue +=
                    noise.noise(
                        PerlinNoise.wrap(mainX * pow),
                        PerlinNoise.wrap(mainY * pow),
                        PerlinNoise.wrap(mainZ * pow),
                        mainSmear * pow,
                        mainY * pow,
                    ) / pow
            }
            pow /= 2.0
        }

        val factor = (mainNoiseValue / 10.0 + 1.0) / 2.0
        val isMax = factor >= 1.0
        val isMin = factor <= 0.0
        var blendMin = 0.0
        var blendMax = 0.0
        pow = 1.0
        for (i in 0 until 16) {
            val wx = PerlinNoise.wrap(limitX * pow)
            val wy = PerlinNoise.wrap(limitY * pow)
            val wz = PerlinNoise.wrap(limitZ * pow)
            val yScalePow = limitSmear * pow
            if (!isMax) {
                val minNoise = minLimitNoise.getOctaveNoise(i)
                if (minNoise != null) blendMin += minNoise.noise(wx, wy, wz, yScalePow, limitY * pow) / pow
            }
            if (!isMin) {
                val maxNoise = maxLimitNoise.getOctaveNoise(i)
                if (maxNoise != null) blendMax += maxNoise.noise(wx, wy, wz, yScalePow, limitY * pow) / pow
            }
            pow /= 2.0
        }

        return Mth.clampedLerp(factor, blendMin / 512.0, blendMax / 512.0) / 128.0
    }

    override fun minValue(): Double = -maxValue()

    override fun maxValue(): Double = cachedMaxValue

    public companion object {
        private const val XZ_YZ_SCALE = 684.412

        public fun createUnseeded(
            xzScale: Double,
            yScale: Double,
            xzFactor: Double,
            yFactor: Double,
            smearScaleMultiplier: Double,
        ): BlendedNoise = BlendedNoise(XoroshiroRandom(0L), xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier)
    }
}
