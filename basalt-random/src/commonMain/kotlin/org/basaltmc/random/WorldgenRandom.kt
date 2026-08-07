package org.basaltmc.random

/**
 * Mirrors vanilla's `net.minecraft.world.level.levelgen.WorldgenRandom`: wraps an inner
 * [BasaltRandom] (typically [XoroshiroRandom] for modern worldgen, occasionally [LegacyRandom])
 * and bridges it to the classic `java.util.Random`-style [BitRandom] algorithms for `nextInt`/
 * `nextFloat`/etc, regardless of which algorithm actually produces the raw bits. Real vanilla
 * does this so worldgen code written against `next(bits)` semantics behaves the same no matter
 * which [BasaltRandom] it is layered over: if the inner source is itself bit-based ([BitRandom],
 * e.g. [LegacyRandom]), `next(bits)` is a pure passthrough; otherwise (e.g. [XoroshiroRandom]) it
 * is synthesized from the inner source's own `nextLong()`, discarding its native bounded-int
 * algorithm entirely. `setSeed` reseeds the inner source directly, never this wrapper's own
 * (unused) `next(bits)` state.
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
