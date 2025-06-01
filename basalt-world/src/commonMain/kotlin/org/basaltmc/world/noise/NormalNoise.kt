package org.basaltmc.world.noise

import org.basaltmc.random.BasaltRandom
import org.basaltmc.world.GenerationMetrics

/**
 * // parity: net.minecraft.world.level.levelgen.synth.NormalNoise. Two independent [PerlinNoise]
 * instances (the second sampled at a slightly different frequency) summed and rescaled to an
 * approximately fixed output deviation, the noise vanilla actually samples for biome climate and
 * density functions (as opposed to raw [PerlinNoise]). Only the modern (non-legacy-Nether-biome)
 * construction path is implemented, same scope decision as [PerlinNoise].
 */
public class NormalNoise private constructor(
    random: BasaltRandom,
    public val parameters: NoiseParameters,
    useNewInitialization: Boolean,
) {
    public class NoiseParameters(
        public val firstOctave: Int,
        public val amplitudes: DoubleArray,
    )

    private val first: PerlinNoise =
        if (useNewInitialization) {
            PerlinNoise.create(random, parameters.firstOctave, parameters.amplitudes)
        } else {
            PerlinNoise.createLegacyForLegacyNetherBiome(random, parameters.firstOctave, parameters.amplitudes)
        }
    private val second: PerlinNoise =
        if (useNewInitialization) {
            PerlinNoise.create(random, parameters.firstOctave, parameters.amplitudes)
        } else {
            PerlinNoise.createLegacyForLegacyNetherBiome(random, parameters.firstOctave, parameters.amplitudes)
        }
    private val valueFactor: Double
    public val maxValue: Double

    init {
        var minOctave = Int.MAX_VALUE
        var maxOctave = Int.MIN_VALUE
        for ((i, amplitude) in parameters.amplitudes.withIndex()) {
            if (amplitude != 0.0) {
                if (i < minOctave) minOctave = i
                if (i > maxOctave) maxOctave = i
            }
        }
        // parity: vanilla's own TARGET_DEVIATION field (1/3) is declared but never actually used
        // here; the constructor divides by a literal 1/6 instead. Kept as-is, bug and all.
        valueFactor = VALUE_FACTOR_NUMERATOR / expectedDeviation(maxOctave - minOctave)
        maxValue = (first.maxValue() + second.maxValue()) * valueFactor
    }

    public fun getValue(
        x: Double,
        y: Double,
        z: Double,
    ): Double {
        if (GenerationMetrics.enabled) GenerationMetrics.countNormalNoise()
        val x2 = x * INPUT_FACTOR
        val y2 = y * INPUT_FACTOR
        val z2 = z * INPUT_FACTOR
        return (first.getValue(x, y, z) + second.getValue(x2, y2, z2)) * valueFactor
    }

    public companion object {
        private const val INPUT_FACTOR = 1.0181268882175227
        private const val VALUE_FACTOR_NUMERATOR = 0.16666666666666666

        public fun create(
            random: BasaltRandom,
            firstOctave: Int,
            amplitudes: DoubleArray,
        ): NormalNoise = NormalNoise(random, NoiseParameters(firstOctave, amplitudes), useNewInitialization = true)

        /** // parity: `NormalNoise.createLegacyNetherBiome`. */
        public fun createLegacyNetherBiome(
            random: BasaltRandom,
            firstOctave: Int,
            amplitudes: DoubleArray,
        ): NormalNoise = NormalNoise(random, NoiseParameters(firstOctave, amplitudes), useNewInitialization = false)

        private fun expectedDeviation(octaveSpan: Int): Double = 0.1 * (1.0 + 1.0 / (octaveSpan + 1))
    }
}
