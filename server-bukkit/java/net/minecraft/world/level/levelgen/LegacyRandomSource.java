// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import java.util.concurrent.atomic.AtomicLong;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public class LegacyRandomSource implements BitRandomSource {
    private static final long MODULUS_MASK = 281474976710655L;
    private static final long MULTIPLIER = 25214903917L;
    private static final long INCREMENT = 11L;
    private final AtomicLong seed = new AtomicLong();
    private final MarsagliaPolarGaussian gaussianSource = new MarsagliaPolarGaussian(this);

    public LegacyRandomSource(long seed) {
        setSeed(seed);
    }

    public long rawState() {
        return seed.get();
    }

    @Override
    public RandomSource fork() {
        return new LegacyRandomSource(nextLong());
    }

    @Override
    public PositionalRandomFactory forkPositional() {
        return new LegacyPositionalRandomFactory(nextLong());
    }

    @Override
    public void setSeed(long seed) {
        this.seed.set((seed ^ MULTIPLIER) & MODULUS_MASK);
        gaussianSource.reset();
    }

    @Override
    public int next(int bits) {
        long newSeed = seed.get() * MULTIPLIER + INCREMENT & MODULUS_MASK;
        seed.set(newSeed);
        return (int) (newSeed >> 48 - bits);
    }

    @Override
    public double nextGaussian() {
        return gaussianSource.nextGaussian();
    }

    public static class LegacyPositionalRandomFactory implements PositionalRandomFactory {
        private final long seed;

        public LegacyPositionalRandomFactory(long seed) {
            this.seed = seed;
        }

        @Override
        public RandomSource at(int x, int y, int z) {
            return new LegacyRandomSource(Mth.getSeed(x, y, z) ^ seed);
        }

        @Override
        public RandomSource fromHashOf(String name) {
            return new LegacyRandomSource((long) name.hashCode() ^ seed);
        }

        @Override
        public RandomSource fromSeed(long seed) {
            return new LegacyRandomSource(seed);
        }

        @Override
        public void parityConfigString(StringBuilder sb) {
            sb.append("LegacyPositionalRandomFactory{").append(seed).append("}");
        }
    }
}
