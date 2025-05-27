package org.basaltmc.world.noise

import org.basaltmc.random.BasaltRandom
import org.basaltmc.random.LegacyRandom
import org.basaltmc.random.WorldgenRandom
import kotlin.math.pow

/**
 * // parity: net.minecraft.world.level.levelgen.synth.PerlinSimplexNoise, octave-summed
 * [SimplexNoise]. Unlike [PerlinNoise] (built from [ImprovedNoise]), octave index `0` here is the
 * *highest* frequency one, not the lowest, and octaves below zero, if present, get freshly reseeded
 * from a `WorldgenRandom`-wrapped `LegacyRandom` derived from the zero octave's own noise value at
 * its own origin, not drawn from [random] directly: matches the real jar's own decompiled body
 * exactly, not a guess at the naming.
 */
public class PerlinSimplexNoise(
    random: BasaltRandom,
    octaves: List<Int>,
) {
    private val noiseLevels: Array<SimplexNoise?>
    private val highestFreqValueFactor: Double
    private val highestFreqInputFactor: Double

    init {
        require(octaves.isNotEmpty()) { "Need some octaves!" }
        val sorted = octaves.distinct().sorted()
        val lowFreqOctaves = -sorted.first()
        val highFreqOctaves = sorted.last()
        val totalOctaves = lowFreqOctaves + highFreqOctaves + 1
        require(totalOctaves >= 1) { "Total number of octaves needs to be >= 1" }

        val zeroOctave = SimplexNoise(random)
        val zeroOctaveIndex = highFreqOctaves
        val levels = arrayOfNulls<SimplexNoise>(totalOctaves)
        if (zeroOctaveIndex in 0 until totalOctaves && sorted.contains(0)) {
            levels[zeroOctaveIndex] = zeroOctave
        }
        for (i in zeroOctaveIndex + 1 until totalOctaves) {
            if (sorted.contains(zeroOctaveIndex - i)) {
                levels[i] = SimplexNoise(random)
            } else {
                random.consumeCount(SKIPPED_OCTAVE_RANDOM_CALLS)
            }
        }

        if (highFreqOctaves > 0) {
            val positiveOctaveSeed =
                (
                    zeroOctave.getValue(
                        zeroOctave.xo,
                        zeroOctave.yo,
                        zeroOctave.zo,
                    ) * 9.223372E18
                ).toLong()
            val highFreqRandom = WorldgenRandom(LegacyRandom(positiveOctaveSeed))
            for (i in zeroOctaveIndex - 1 downTo 0) {
                if (i < totalOctaves && sorted.contains(zeroOctaveIndex - i)) {
                    levels[i] = SimplexNoise(highFreqRandom)
                } else {
                    highFreqRandom.consumeCount(SKIPPED_OCTAVE_RANDOM_CALLS)
                }
            }
        }

        noiseLevels = levels
        highestFreqInputFactor = 2.0.pow(highFreqOctaves)
        highestFreqValueFactor = 1.0 / (2.0.pow(totalOctaves) - 1.0)
    }

    public fun getValue(
        x: Double,
        y: Double,
        useNoiseStart: Boolean = false,
    ): Double {
        var value = 0.0
        var factor = highestFreqInputFactor
        var valueFactor = highestFreqValueFactor
        for (noise in noiseLevels) {
            if (noise != null) {
                val ox = if (useNoiseStart) noise.xo else 0.0
                val oy = if (useNoiseStart) noise.yo else 0.0
                value += noise.getValue(x * factor + ox, y * factor + oy) * valueFactor
            }
            factor /= 2.0
            valueFactor *= 2.0
        }
        return value
    }

    private companion object {
        const val SKIPPED_OCTAVE_RANDOM_CALLS = 262
    }
}
