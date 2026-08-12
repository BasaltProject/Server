package org.basaltmc.server.random

import kotlin.math.ln
import kotlin.math.sqrt

/**
 * Mirrors vanilla's `net.minecraft.world.level.levelgen.MarsagliaPolarGaussian`: the polar
 * (Marsaglia) method for sampling a standard normal distribution, caching the spare sample.
 */
public class MarsagliaPolarGaussian(
    private val source: BasaltRandom,
) {
    private var nextNextGaussian: Double = 0.0
    private var haveNextNextGaussian: Boolean = false

    public fun reset() {
        haveNextNextGaussian = false
    }

    public fun nextGaussian(): Double {
        if (haveNextNextGaussian) {
            haveNextNextGaussian = false
            return nextNextGaussian
        }

        var x: Double
        var y: Double
        var radiusSquared: Double
        do {
            x = 2.0 * source.nextDouble() - 1.0
            y = 2.0 * source.nextDouble() - 1.0
            radiusSquared = x * x + y * y
        } while (radiusSquared >= 1.0 || radiusSquared == 0.0)

        val multiplier = sqrt(-2.0 * ln(radiusSquared) / radiusSquared)
        nextNextGaussian = y * multiplier
        haveNextNextGaussian = true
        return x * multiplier
    }
}
