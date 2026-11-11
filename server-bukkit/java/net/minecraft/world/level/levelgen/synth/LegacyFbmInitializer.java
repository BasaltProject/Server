// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import it.unimi.dsi.fastutil.doubles.DoubleList;
import net.minecraft.util.RandomSource;

@Deprecated
public class LegacyFbmInitializer {
    public static NoiseStack createForLegacyNetherBiome(RandomSource random, int firstOctave, DoubleList amplitudes) {
        int octaves = amplitudes.size();
        int zeroIndex = -firstOctave;
        PerlinNoise[] levels = new PerlinNoise[octaves];
        PerlinNoise zero = new PerlinNoise(random);
        if (zeroIndex >= 0 && zeroIndex < octaves && amplitudes.getDouble(zeroIndex) != 0.0) {
            levels[zeroIndex] = zero;
        }
        for (int i = zeroIndex - 1; i >= 0; i--) {
            if (i < octaves && amplitudes.getDouble(i) != 0.0) {
                levels[i] = new PerlinNoise(random);
            } else {
                random.consumeCount(262);
            }
        }
        long present = 0;
        long expected = 0;
        for (int i = 0; i < octaves; i++) {
            present += levels[i] != null ? 1 : 0;
            expected += amplitudes.getDouble(i) != 0.0 ? 1 : 0;
        }
        if (present != expected) {
            throw new IllegalStateException("Failed to create correct number of noise levels for given non-zero amplitudes");
        }
        if (zeroIndex < octaves - 1) {
            throw new IllegalArgumentException("Positive octaves are temporarily disabled");
        }
        double frequency = Math.pow(2.0, -zeroIndex);
        double weight = Math.pow(2.0, octaves - 1) / (Math.pow(2.0, octaves) - 1.0);
        NoiseStack.Builder stack = NoiseStack.builder();
        for (int i = 0; i < levels.length; i++) {
            if (levels[i] != null) {
                stack.add(levels[i], frequency, (float) (weight * amplitudes.getDouble(i)));
            }
            frequency *= 2.0;
            weight /= 2.0;
        }
        return stack.build();
    }
}
