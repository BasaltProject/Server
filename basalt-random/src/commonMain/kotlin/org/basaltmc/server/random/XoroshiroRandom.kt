package org.basaltmc.server.random

/**
 * Mirrors vanilla's `net.minecraft.world.level.levelgen.XoroshiroRandomSource`: xoroshiro128++
 * as the bit source, with its own (non-`BitRandomSource`) bounded-int algorithm (Lemire's
 * method) rather than java.util.Random-style rejection sampling. Golden vectors for this class
 * were captured by running the actual decompiled 26.2 bytecode; see basalt-random tests.
 */
public class XoroshiroRandom private constructor(
    private var generator: Xoroshiro128PlusPlus,
) : BasaltRandom {
    private val gaussianSource = MarsagliaPolarGaussian(this)

    public constructor(seed: Long) : this(Xoroshiro128PlusPlus(RandomSupport.upgradeSeedTo128bit(seed)))

    public constructor(seed: Seed128bit) : this(Xoroshiro128PlusPlus(seed))

    public constructor(seedLo: Long, seedHi: Long) : this(Xoroshiro128PlusPlus(seedLo, seedHi))

    override fun fork(): BasaltRandom = XoroshiroRandom(generator.nextLong(), generator.nextLong())

    override fun forkPositional(): PositionalRandomFactory =
        XoroshiroPositionalRandomFactory(generator.nextLong(), generator.nextLong())

    override fun setSeed(seed: Long) {
        generator = Xoroshiro128PlusPlus(RandomSupport.upgradeSeedTo128bit(seed))
        gaussianSource.reset()
    }

    override fun nextInt(): Int = generator.nextLong().toInt()

    override fun nextInt(bound: Int): Int {
        require(bound > 0) { "Bound must be positive" }

        var randomBits = nextInt().toLong() and 0xFFFFFFFFL
        var multipliedRandomBits = randomBits * bound
        var fractionalPart = multipliedRandomBits and 0xFFFFFFFFL
        if (fractionalPart < bound) {
            val unbiasedBucketsStartIndex = ((-bound).toLong() and 0xFFFFFFFFL) % bound
            while (fractionalPart < unbiasedBucketsStartIndex) {
                randomBits = nextInt().toLong() and 0xFFFFFFFFL
                multipliedRandomBits = randomBits * bound
                fractionalPart = multipliedRandomBits and 0xFFFFFFFFL
            }
        }
        return (multipliedRandomBits shr 32).toInt()
    }

    override fun nextLong(): Long = generator.nextLong()

    override fun nextBoolean(): Boolean = (generator.nextLong() and 1L) != 0L

    override fun nextFloat(): Float = nextBits(24).toFloat() * FLOAT_MULTIPLIER

    override fun nextDouble(): Double = nextBits(53) * DOUBLE_MULTIPLIER

    override fun nextGaussian(): Double = gaussianSource.nextGaussian()

    override fun consumeCount(rounds: Int) {
        for (i in 0 until rounds) generator.nextLong()
    }

    private fun nextBits(bits: Int): Long = generator.nextLong() ushr (64 - bits)

    private companion object {
        const val FLOAT_MULTIPLIER: Float = 5.9604645E-8F
        const val DOUBLE_MULTIPLIER: Double = 1.110223E-16F.toDouble()
    }
}

/** Mirrors `XoroshiroRandomSource.XoroshiroPositionalRandomFactory`. */
public class XoroshiroPositionalRandomFactory(
    private val seedLo: Long,
    private val seedHi: Long,
) : PositionalRandomFactory {
    override fun at(
        x: Int,
        y: Int,
        z: Int,
    ): BasaltRandom {
        val positionalSeed = PositionalHashing.getSeed(x, y, z)
        return XoroshiroRandom(positionalSeed xor seedLo, seedHi)
    }

    override fun fromHashOf(name: String): BasaltRandom {
        val seed = RandomSupport.seedFromHashOf(name)
        return XoroshiroRandom(seed.xor(seedLo, seedHi))
    }

    override fun fromSeed(seed: Long): BasaltRandom = XoroshiroRandom(seed xor seedLo, seed xor seedHi)
}
