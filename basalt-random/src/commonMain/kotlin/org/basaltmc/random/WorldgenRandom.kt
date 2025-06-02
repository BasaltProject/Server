package org.basaltmc.random

/**
 * // parity: net.minecraft.world.level.levelgen.WorldgenRandom. Wraps an inner [BasaltRandom] and
 * bridges it to the `java.util.Random`-style [BitRandom] algorithms, so worldgen written against
 * `next(bits)` behaves the same over any source.
 *
 * Over a bit-based source like [LegacyRandom] that is a passthrough; over [XoroshiroRandom] the
 * bits are synthesized from `nextLong()`, discarding its own bounded-int algorithm. `setSeed`
 * reseeds the inner source, never this wrapper's unused state.
 */
public class WorldgenRandom(
    private val inner: BasaltRandom,
) : BitRandom {
    private val gaussianSource = MarsagliaPolarGaussian(this)

    override fun next(bits: Int): Int =
        if (inner is BitRandom) {
            inner.next(bits)
        } else {
            (inner.nextLong() ushr (64 - bits)).toInt()
        }

    override fun setSeed(seed: Long) {
        inner.setSeed(seed)
        gaussianSource.reset()
    }

    override fun fork(): BasaltRandom = inner.fork()

    override fun forkPositional(): PositionalRandomFactory = inner.forkPositional()

    override fun nextGaussian(): Double = gaussianSource.nextGaussian()

    /** // parity: WorldgenRandom.setDecorationSeed. [chunkX]/[chunkZ] are block coordinates (the chunk's origin, `chunkX shl 4`), not chunk coordinates. */
    public fun setDecorationSeed(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ): Long {
        setSeed(seed)
        val xScale = nextLong() or 1L
        val zScale = nextLong() or 1L
        val result = (chunkX.toLong() * xScale + chunkZ.toLong() * zScale) xor seed
        setSeed(result)
        return result
    }

    /** // parity: WorldgenRandom.setFeatureSeed. */
    public fun setFeatureSeed(
        seed: Long,
        index: Int,
        step: Int,
    ) {
        setSeed(seed + index + 10000L * step)
    }

    /** // parity: WorldgenRandom.setLargeFeatureSeed. */
    public fun setLargeFeatureSeed(
        seed: Long,
        chunkX: Int,
        chunkZ: Int,
    ) {
        setSeed(seed)
        val xScale = nextLong()
        val zScale = nextLong()
        setSeed((chunkX.toLong() * xScale) xor (chunkZ.toLong() * zScale) xor seed)
    }

    /** // parity: WorldgenRandom.setLargeFeatureWithSalt. */
    public fun setLargeFeatureWithSalt(
        seed: Long,
        x: Int,
        z: Int,
        blend: Int,
    ) {
        val result = x.toLong() * 341873128712L + z.toLong() * 132897987541L + seed + blend
        setSeed(result)
    }
}
