package org.basaltmc.bukkit.facade;

import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import org.basaltmc.bukkit.WorldEnvNatives;

/** A named random sequence of a world; every draw is taken from the native sequence so the stream is shared. */
public final class NativeSequenceRandom implements RandomSource {
    private final String world;
    private final String id;

    public NativeSequenceRandom(String world, String id) {
        this.world = world;
        this.id = id;
    }

    private long draw(int operation, long argument) {
        return WorldEnvNatives.randomSequence(world, id, operation, argument);
    }

    public RandomSource fork() {
        return new XoroshiroRandomSource(nextLong());
    }

    public PositionalRandomFactory forkPositional() {
        return new XoroshiroRandomSource(nextLong()).forkPositional();
    }

    public void setSeed(long seed) {
        draw(7, 0);
    }

    public int nextInt() {
        return (int) draw(0, 0);
    }

    public int nextInt(int bound) {
        return (int) draw(1, bound);
    }

    public long nextLong() {
        return draw(2, 0);
    }

    public boolean nextBoolean() {
        return draw(3, 0) != 0;
    }

    public float nextFloat() {
        return Float.intBitsToFloat((int) draw(4, 0));
    }

    public double nextDouble() {
        return Double.longBitsToDouble(draw(5, 0));
    }

    public double nextGaussian() {
        return Double.longBitsToDouble(draw(6, 0));
    }
}
