package org.basaltmc.server.random

/**
 * Contract mirrored from vanilla's `net.minecraft.util.RandomSource`. Implementations must be
 * bit-exact with vanilla: same seed, same algorithm, same sequence of values.
 */
public interface BasaltRandom {
    public fun fork(): BasaltRandom

    public fun forkPositional(): PositionalRandomFactory

    public fun setSeed(seed: Long)

    public fun nextInt(): Int

    public fun nextInt(bound: Int): Int

    public fun nextLong(): Long

    public fun nextBoolean(): Boolean

    public fun nextFloat(): Float

    public fun nextDouble(): Double

    public fun nextGaussian(): Double

    public fun nextInt(
        origin: Int,
        bound: Int,
    ): Int {
        require(origin < bound) { "bound - origin is non positive" }
        return origin + nextInt(bound - origin)
    }

    public fun nextIntBetweenInclusive(
        min: Int,
        maxInclusive: Int,
    ): Int = nextInt(maxInclusive - min + 1) + min

    public fun triangle(
        mean: Double,
        spread: Double,
    ): Double = mean + spread * (nextDouble() - nextDouble())

    public fun triangle(
        mean: Float,
        spread: Float,
    ): Float = mean + spread * (nextFloat() - nextFloat())

    public fun consumeCount(rounds: Int) {
        for (i in 0 until rounds) nextInt()
    }
}
