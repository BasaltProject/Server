package org.basaltmc.random

/**
 * Mirrors vanilla's `net.minecraft.world.level.levelgen.BitRandomSource`: a [BasaltRandom]
 * whose every operation is derived from a single `next(bits)` primitive, java.util.Random-style.
 */
public interface BitRandom : BasaltRandom {
    public fun next(bits: Int): Int

    override fun nextInt(): Int = next(32)

    override fun nextInt(bound: Int): Int {
        require(bound > 0) { "Bound must be positive" }

        if ((bound and (bound - 1)) == 0) {
            return ((bound.toLong() * next(31)) shr 31).toInt()
        }

        var sample: Int
        var modulo: Int
        do {
            sample = next(31)
            modulo = sample % bound
        } while (sample - modulo + (bound - 1) < 0)
        return modulo
    }

    override fun nextLong(): Long {
        val upper = next(32)
        val lower = next(32)
        return (upper.toLong() shl 32) + lower
    }

    override fun nextBoolean(): Boolean = next(1) != 0

    override fun nextFloat(): Float = next(24) * FLOAT_MULTIPLIER

    override fun nextDouble(): Double {
        val upper = next(26)
        val lower = next(27)
        val combined = (upper.toLong() shl 27) + lower
        return combined * DOUBLE_MULTIPLIER
    }

    private companion object {
        const val FLOAT_MULTIPLIER: Float = 5.9604645E-8F
        const val DOUBLE_MULTIPLIER: Double = 1.110223E-16F.toDouble()
    }
}
