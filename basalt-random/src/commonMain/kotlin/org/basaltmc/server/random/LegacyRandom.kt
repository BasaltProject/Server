package org.basaltmc.server.random

/**
 * Mirrors vanilla's `net.minecraft.world.level.levelgen.LegacyRandomSource`: the same 48-bit
 * linear congruential generator as `java.util.Random`. Golden vectors for this class were
 * captured by running the actual decompiled 26.2 bytecode; see basalt-random tests.
 *
 * Vanilla guards concurrent misuse of the seed with an `AtomicLong` + `ThreadingDetector` that
 * throws on a racing `compareAndSet`. That guard is a debug safety net, not part of the
 * observable sequence, so it is intentionally not reproduced here: only observable behavior is
 * in scope. [LegacyRandom] instances are not meant to be shared across threads.
 */
public class LegacyRandom(
    seed: Long,
) : BitRandom {
    private var seed: Long = 0L
    private val gaussianSource = MarsagliaPolarGaussian(this)

    init {
        setSeed(seed)
    }

    override fun fork(): BasaltRandom = LegacyRandom(nextLong())

    override fun forkPositional(): PositionalRandomFactory = LegacyPositionalRandomFactory(nextLong())

    override fun setSeed(seed: Long) {
        this.seed = (seed xor MULTIPLIER) and MODULUS_MASK
        gaussianSource.reset()
    }

    override fun next(bits: Int): Int {
        val newSeed = (seed * MULTIPLIER + INCREMENT) and MODULUS_MASK
        seed = newSeed
        return (newSeed ushr (48 - bits)).toInt()
    }

    override fun nextGaussian(): Double = gaussianSource.nextGaussian()

    private companion object {
        const val MODULUS_MASK: Long = 281474976710655L
        const val MULTIPLIER: Long = 25214903917L
        const val INCREMENT: Long = 11L
    }
}

/** Mirrors `LegacyRandomSource.LegacyPositionalRandomFactory`. */
public class LegacyPositionalRandomFactory(
    private val seed: Long,
) : PositionalRandomFactory {
    override fun at(
        x: Int,
        y: Int,
        z: Int,
    ): BasaltRandom {
        val positionalSeed = PositionalHashing.getSeed(x, y, z)
        return LegacyRandom(positionalSeed xor seed)
    }

    override fun fromHashOf(name: String): BasaltRandom {
        val positionalSeed = javaStringHashCode(name)
        return LegacyRandom(positionalSeed.toLong() xor seed)
    }

    override fun fromSeed(seed: Long): BasaltRandom = LegacyRandom(seed)
}

/**
 * Reproduces `java.lang.String.hashCode()` (`s[0]*31^(n-1) + ... + s[n-1]`), used by
 * `LegacyPositionalRandomFactory.fromHashOf`. Kotlin/Native's `String.hashCode()` is not
 * guaranteed to use this algorithm, so it cannot be relied on for parity.
 */
internal fun javaStringHashCode(value: String): Int {
    var hash = 0
    for (char in value) {
        hash = 31 * hash + char.code
    }
    return hash
}
