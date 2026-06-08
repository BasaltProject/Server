package net.minecraft.util;

import java.util.Random;

public interface RandomSource {
    static RandomSource create() {
        return new Sequence(new Random());
    }

    static RandomSource create(long seed) {
        return new Sequence(new Random(seed));
    }

    int nextInt();

    int nextInt(int bound);

    long nextLong();

    boolean nextBoolean();

    float nextFloat();

    double nextDouble();

    double nextGaussian();

    final class Sequence implements RandomSource {
        private final Random random;

        private Sequence(Random random) {
            this.random = random;
        }

        @Override
        public int nextInt() {
            return random.nextInt();
        }

        @Override
        public int nextInt(int bound) {
            return random.nextInt(bound);
        }

        @Override
        public long nextLong() {
            return random.nextLong();
        }

        @Override
        public boolean nextBoolean() {
            return random.nextBoolean();
        }

        @Override
        public float nextFloat() {
            return random.nextFloat();
        }

        @Override
        public double nextDouble() {
            return random.nextDouble();
        }

        @Override
        public double nextGaussian() {
            return random.nextGaussian();
        }
    }
}
