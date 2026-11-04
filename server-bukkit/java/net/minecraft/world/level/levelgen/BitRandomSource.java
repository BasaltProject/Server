// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.util.RandomSource;

public interface BitRandomSource extends RandomSource {
    float FLOAT_MULTIPLIER = 5.9604645E-8F;
    double DOUBLE_MULTIPLIER = (double) 1.110223E-16F;

    int next(int bits);

    @Override
    default int nextInt() {
        return next(32);
    }

    @Override
    default int nextInt(int bound) {
        if (bound <= 0) {
            throw new IllegalArgumentException("Bound must be positive");
        }
        if ((bound & bound - 1) == 0) {
            return (int) ((long) bound * (long) next(31) >> 31);
        }
        int sample;
        int modulo;
        do {
            sample = next(31);
            modulo = sample % bound;
        } while (sample - modulo + (bound - 1) < 0);
        return modulo;
    }

    @Override
    default long nextLong() {
        int upper = next(32);
        int lower = next(32);
        return ((long) upper << 32) + (long) lower;
    }

    @Override
    default boolean nextBoolean() {
        return next(1) != 0;
    }

    @Override
    default float nextFloat() {
        return (float) next(24) * FLOAT_MULTIPLIER;
    }

    @Override
    default double nextDouble() {
        int upper = next(26);
        int lower = next(27);
        long combined = ((long) upper << 27) + (long) lower;
        return (double) combined * DOUBLE_MULTIPLIER;
    }
}
