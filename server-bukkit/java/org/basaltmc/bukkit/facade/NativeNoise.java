package org.basaltmc.bukkit.facade;

import java.lang.ref.Cleaner;
import net.minecraft.util.Interval;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.levelgen.synth.Noise;
import org.basaltmc.bukkit.LeftoverNatives;

/** A noise held by the native noise service; sampling goes to the native code, so the values match the game's. */
public final class NativeNoise implements Noise {
    private static final Cleaner kCleaner = Cleaner.create();
    private final long handle;
    private Interval range;

    private NativeNoise(long handle) {
        this.handle = handle;
        kCleaner.register(this, () -> LeftoverNatives.noiseRelease(handle));
    }

    /** The noise of the game data under this name, seeded from the current state of the random source. */
    public static NativeNoise create(String id, RandomSource random, boolean legacyNether) {
        return of(random, (legacy, low, high) -> LeftoverNatives.noiseCreate(id, legacy, low, high, legacyNether));
    }

    public static NativeNoise create(double baseAmplitude, int baseOctave, int octaveCount, int normalize,
        double[] modifiers, RandomSource random, boolean legacyNether) {
        return of(random, (legacy, low, high) -> LeftoverNatives.noiseCreateWith(baseAmplitude, baseOctave,
            octaveCount, normalize, modifiers, legacy, low, high, legacyNether));
    }

    private interface Maker {
        long make(boolean legacy, long low, long high);
    }

    private static NativeNoise of(RandomSource random, Maker maker) {
        RandomSource source = random instanceof WorldgenRandom wrapped ? wrapped.inner() : random;
        long handle;
        if (source instanceof XoroshiroRandomSource xoroshiro) {
            Xoroshiro128PlusPlus state = xoroshiro.generatorState();
            handle = maker.make(false, state.stateLow(), state.stateHigh());
        } else if (source instanceof LegacyRandomSource legacy) {
            handle = maker.make(true, legacy.rawState(), 0L);
        } else {
            throw new IllegalArgumentException("A native noise needs a vanilla random source");
        }
        if (handle < 0) {
            throw new IllegalStateException("The native noise could not be created");
        }
        return new NativeNoise(handle);
    }

    public Interval range() {
        if (range == null) {
            double[] bounds = LeftoverNatives.noiseHandleRange(handle);
            range = bounds == null ? Interval.INFINITE : Interval.of((float) bounds[0], (float) bounds[1]);
        }
        return range;
    }

    public float get(double x, double y) {
        return (float) LeftoverNatives.noiseSample(handle, x, y, Double.NaN);
    }

    public float get(double x, double y, double z) {
        return (float) LeftoverNatives.noiseSample(handle, x, y, z);
    }
}
