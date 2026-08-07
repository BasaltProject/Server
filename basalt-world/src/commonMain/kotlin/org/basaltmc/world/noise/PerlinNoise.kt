package org.basaltmc.world.noise

import org.basaltmc.common.Mth
import org.basaltmc.random.BasaltRandom
import kotlin.math.pow

/**
 * // parity: net.minecraft.world.level.levelgen.synth.PerlinNoise, octave-summed [ImprovedNoise].
 * Two construction paths, matching vanilla's own `useNewInitialization` split: [create] (one
 * [ImprovedNoise] per octave seeded via [BasaltRandom.forkPositional], what every modern noise
 * reference uses) and [createLegacyForBlendedNoise] (octaves consumed sequentially off a single
 * shared [BasaltRandom], skipping 262 draws for a zero-amplitude octave to keep the sequence
 * aligned; needed by [org.basaltmc.world.densityfunction.BlendedNoise], vanilla's base 3D terrain
 * noise, which still uses this legacy path even in 26.2). `createLegacyForLegacyNetherBiome`
 * (pre-1.18 Nether biome noise, unrelated to `BlendedNoise`) is still not implemented: nothing in
 * this project generates pre-1.18-style Nether biomes.
 */
public class PerlinNoise private constructor(
    random: BasaltRandom,
    firstOctave: Int,
    amplitudes: DoubleArray,
    useNewInitialization: Boolean,
) {
    private val firstOctave: Int = firstOctave
    private val amplitudes: DoubleArray = amplitudes
    private val noiseLevels: Array<ImprovedNoise?>
    private val lowestFreqInputFactor: Double
    private val lowestFreqValueFactor: Double
    private val maxValue: Double

    init {
        val octaves = amplitudes.size
        val zeroOctaveIndex = -firstOctave
        noiseLevels =
            if (useNewInitialization) {
                val positional = random.forkPositional()
                Array(octaves) { i ->
                    if (amplitudes[i] !=
                        0.0
                    ) {
                        positional.fromHashOf("octave_${firstOctave + i}").let(::ImprovedNoise)
                    } else {
                        null
                    }
                }
            } else {
                buildLegacyNoiseLevels(random, octaves, zeroOctaveIndex)
            }
        lowestFreqInputFactor = 2.0.pow(-zeroOctaveIndex)
        lowestFreqValueFactor = 2.0.pow(octaves - 1) / (2.0.pow(octaves) - 1.0)
        maxValue = edgeValue(2.0)
    }

    private fun buildLegacyNoiseLevels(
        random: BasaltRandom,
        octaves: Int,
        zeroOctaveIndex: Int,
    ): Array<ImprovedNoise?> {
        val levels = arrayOfNulls<ImprovedNoise>(octaves)
        val zeroOctave = ImprovedNoise(random)
        if (zeroOctaveIndex in 0 until octaves && amplitudes[zeroOctaveIndex] != 0.0) {
            levels[zeroOctaveIndex] = zeroOctave
        }
        for (i in zeroOctaveIndex - 1 downTo 0) {
            if (i < octaves && amplitudes[i] != 0.0) {
                levels[i] = ImprovedNoise(random)
            } else {
                random.consumeCount(SKIPPED_OCTAVE_RANDOM_CALLS)
            }
        }
        return levels
    }

    public fun maxValue(): Double = maxValue

    /** // parity: PerlinNoise.maxBrokenValue. The bound vanilla actually uses for `BlendedNoise`'s min/max, not [maxValue]. */
    public fun maxBrokenValue(yScale: Double): Double = edgeValue(yScale + 2.0)

    /** The octave [i] levels away from the highest frequency one (index `0`), or `null` if that octave has zero amplitude. */
    public fun getOctaveNoise(i: Int): ImprovedNoise? = noiseLevels[noiseLevels.size - 1 - i]

    public fun getValue(
        x: Double,
        y: Double,
        z: Double,
    ): Double {
        var value = 0.0
        var factor = lowestFreqInputFactor
        var valueFactor = lowestFreqValueFactor
        for (i in noiseLevels.indices) {
            val noise = noiseLevels[i]
            if (noise != null) {
                val noiseVal = noise.noise(wrap(x * factor), wrap(y * factor), wrap(z * factor), 0.0, 0.0)
                value += amplitudes[i] * noiseVal * valueFactor
            }
            factor *= 2.0
            valueFactor /= 2.0
        }
        return value
    }

    private fun edgeValue(noiseValue: Double): Double {
        var value = 0.0
        var valueFactor = lowestFreqValueFactor
        for (i in noiseLevels.indices) {
            if (noiseLevels[i] != null) value += amplitudes[i] * noiseValue * valueFactor
            valueFactor /= 2.0
        }
        return value
    }

    public companion object {
        private const val ROUND_OFF = 33554432.0
        private const val SKIPPED_OCTAVE_RANDOM_CALLS = 262

        public fun create(
            random: BasaltRandom,
            firstOctave: Int,
            amplitudes: DoubleArray,
        ): PerlinNoise = PerlinNoise(random, firstOctave, amplitudes, useNewInitialization = true)

        /** [octaves] is the sorted, distinct set of octave indices that get amplitude 1.0; every other octave in the span is 0.0. */
        public fun create(
            random: BasaltRandom,
            octaves: List<Int>,
        ): PerlinNoise {
            val (firstOctave, amplitudes) = makeAmplitudes(octaves)
            return PerlinNoise(random, firstOctave, amplitudes, useNewInitialization = true)
        }

        /** Same octave/amplitude shape as [create], but seeded off [random] directly instead of via [BasaltRandom.forkPositional]. */
        public fun createLegacyForBlendedNoise(
            random: BasaltRandom,
            octaves: List<Int>,
        ): PerlinNoise {
            val (firstOctave, amplitudes) = makeAmplitudes(octaves)
            return PerlinNoise(random, firstOctave, amplitudes, useNewInitialization = false)
        }

        public fun wrap(x: Double): Double = x - Mth.lfloor(x / ROUND_OFF + 0.5) * ROUND_OFF

        private fun makeAmplitudes(octaves: List<Int>): Pair<Int, DoubleArray> {
            require(octaves.isNotEmpty()) { "Need some octaves!" }
            val lowFreqOctaves = -octaves.min()
            val highFreqOctaves = octaves.max()
            val span = lowFreqOctaves + highFreqOctaves + 1
            require(span >= 1) { "Total number of octaves needs to be >= 1" }
            val amplitudes = DoubleArray(span)
            for (octave in octaves) amplitudes[octave + lowFreqOctaves] = 1.0
            return -lowFreqOctaves to amplitudes
        }
    }
}
