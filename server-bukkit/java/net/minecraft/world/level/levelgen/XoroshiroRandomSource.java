// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import com.mojang.serialization.Codec;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public class XoroshiroRandomSource implements RandomSource {
    private static final float FLOAT_UNIT = 5.9604645E-8F;
    private static final double DOUBLE_UNIT = (double) 1.110223E-16F;
    public static final Codec<XoroshiroRandomSource> CODEC =
        Xoroshiro128PlusPlus.CODEC.xmap(generator -> new XoroshiroRandomSource(generator), source -> source.generator);
    private Xoroshiro128PlusPlus generator;
    private final MarsagliaPolarGaussian gaussianSource = new MarsagliaPolarGaussian(this);

    public XoroshiroRandomSource(long seed) {
        generator = new Xoroshiro128PlusPlus(RandomSupport.upgradeSeedTo128bit(seed));
    }

    public XoroshiroRandomSource(RandomSupport.Seed128bit seed) {
        generator = new Xoroshiro128PlusPlus(seed);
    }

    public XoroshiroRandomSource(long seedLo, long seedHi) {
        generator = new Xoroshiro128PlusPlus(seedLo, seedHi);
    }

    private XoroshiroRandomSource(Xoroshiro128PlusPlus generator) {
        this.generator = generator;
    }

    @Override
    public RandomSource fork() {
        return new XoroshiroRandomSource(generator.nextLong(), generator.nextLong());
    }

    @Override
    public PositionalRandomFactory forkPositional() {
        return new XoroshiroPositionalRandomFactory(generator.nextLong(), generator.nextLong());
    }

    @Override
    public void setSeed(long seed) {
        generator = new Xoroshiro128PlusPlus(RandomSupport.upgradeSeedTo128bit(seed));
        gaussianSource.reset();
    }

    @Override
    public int nextInt() {
        return (int) generator.nextLong();
    }

    @Override
    public int nextInt(int bound) {
        if (bound <= 0) {
            throw new IllegalArgumentException("Bound must be positive");
        }
        long randomBits = Integer.toUnsignedLong(nextInt());
        long multiplied = randomBits * (long) bound;
        long fraction = multiplied & 4294967295L;
        if (fraction < (long) bound) {
            int threshold = Integer.remainderUnsigned(~bound + 1, bound);
            while (fraction < (long) threshold) {
                randomBits = Integer.toUnsignedLong(nextInt());
                multiplied = randomBits * (long) bound;
                fraction = multiplied & 4294967295L;
            }
        }
        return (int) (multiplied >> 32);
    }

    @Override
    public long nextLong() {
        return generator.nextLong();
    }

    @Override
    public boolean nextBoolean() {
        return (generator.nextLong() & 1L) != 0L;
    }

    @Override
    public float nextFloat() {
        return (float) nextBits(24) * FLOAT_UNIT;
    }

    @Override
    public double nextDouble() {
        return (double) nextBits(53) * DOUBLE_UNIT;
    }

    @Override
    public double nextGaussian() {
        return gaussianSource.nextGaussian();
    }

    @Override
    public void consumeCount(int rounds) {
        for (int i = 0; i < rounds; i++) {
            generator.nextLong();
        }
    }

    private long nextBits(int bits) {
        return generator.nextLong() >>> 64 - bits;
    }

    public static class XoroshiroPositionalRandomFactory implements PositionalRandomFactory {
        private final long seedLo;
        private final long seedHi;

        public XoroshiroPositionalRandomFactory(long seedLo, long seedHi) {
            this.seedLo = seedLo;
            this.seedHi = seedHi;
        }

        @Override
        public RandomSource at(int x, int y, int z) {
            return new XoroshiroRandomSource(Mth.getSeed(x, y, z) ^ seedLo, seedHi);
        }

        @Override
        public RandomSource fromHashOf(String name) {
            return new XoroshiroRandomSource(RandomSupport.seedFromHashOf(name).xor(seedLo, seedHi));
        }

        @Override
        public RandomSource fromSeed(long seed) {
            return new XoroshiroRandomSource(seed ^ seedLo, seed ^ seedHi);
        }

        @Override
        public void parityConfigString(StringBuilder sb) {
            sb.append("seedLo: ").append(seedLo).append(", seedHi: ").append(seedHi);
        }
    }
}
