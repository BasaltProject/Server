// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.util.RandomSource;

public class SingleThreadedRandomSource implements BitRandomSource {
    private static final long MODULUS_MASK = 281474976710655L;
    private static final long MULTIPLIER = 25214903917L;
    private static final long INCREMENT = 11L;
    private long seed;
    private MarsagliaPolarGaussian gaussianSource;

    public SingleThreadedRandomSource(long seed) {
        setSeed(seed);
    }

    @Override
    public RandomSource fork() {
        return new SingleThreadedRandomSource(nextLong());
    }

    @Override
    public PositionalRandomFactory forkPositional() {
        return new LegacyRandomSource.LegacyPositionalRandomFactory(nextLong());
    }

    @Override
    public void setSeed(long seed) {
        this.seed = (seed ^ MULTIPLIER) & MODULUS_MASK;
        if (gaussianSource != null) {
            gaussianSource.reset();
        }
    }

    @Override
    public int next(int bits) {
        long newSeed = seed * MULTIPLIER + INCREMENT & MODULUS_MASK;
        seed = newSeed;
        return (int) (newSeed >> 48 - bits);
    }

    @Override
    public double nextGaussian() {
        if (gaussianSource == null) {
            gaussianSource = new MarsagliaPolarGaussian(this);
        }
        return gaussianSource.nextGaussian();
    }
}
