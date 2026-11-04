// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import java.util.function.LongFunction;
import net.minecraft.util.RandomSource;

public class WorldgenRandom extends LegacyRandomSource {
    private final RandomSource randomSource;
    private int count;

    public WorldgenRandom(RandomSource randomSource) {
        super(0L);
        this.randomSource = randomSource;
    }

    public int getCount() {
        return count;
    }

    @Override
    public RandomSource fork() {
        return randomSource.fork();
    }

    @Override
    public PositionalRandomFactory forkPositional() {
        return randomSource.forkPositional();
    }

    @Override
    public int next(int bits) {
        count++;
        if (randomSource instanceof LegacyRandomSource legacy) {
            return legacy.next(bits);
        }
        return (int) (randomSource.nextLong() >>> 64 - bits);
    }

    @Override
    public synchronized void setSeed(long seed) {
        if (randomSource != null) {
            randomSource.setSeed(seed);
        }
    }

    public long setDecorationSeed(long seed, int chunkX, int chunkZ) {
        setSeed(seed);
        long xScale = nextLong() | 1L;
        long zScale = nextLong() | 1L;
        long result = (long) chunkX * xScale + (long) chunkZ * zScale ^ seed;
        setSeed(result);
        return result;
    }

    public void setFeatureSeed(long seed, int index, int step) {
        setSeed(seed + (long) index + (long) (10000 * step));
    }

    public void setLargeFeatureSeed(long seed, int chunkX, int chunkZ) {
        setSeed(seed);
        long xScale = nextLong();
        long zScale = nextLong();
        setSeed((long) chunkX * xScale ^ (long) chunkZ * zScale ^ seed);
    }

    public void setLargeFeatureWithSalt(long seed, int x, int z, int blend) {
        setSeed((long) x * 341873128712L + (long) z * 132897987541L + seed + (long) blend);
    }

    public static RandomSource seedSlimeChunk(int x, int z, long seed, long salt) {
        return RandomSource.createThreadLocalInstance(
            seed + (long) (x * x * 4987142) + (long) (x * 5947611) + (long) (z * z) * 4392871L + (long) (z * 389711) ^ salt);
    }

    public enum Algorithm {
        LEGACY(LegacyRandomSource::new),
        XOROSHIRO(XoroshiroRandomSource::new);

        private final LongFunction<RandomSource> constructor;

        Algorithm(LongFunction<RandomSource> constructor) {
            this.constructor = constructor;
        }

        public RandomSource newInstance(long seed) {
            return constructor.apply(seed);
        }
    }
}
