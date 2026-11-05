// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.synth.Noise;
import net.minecraft.world.level.levelgen.synth.SimplexNoise;

final class BiomeNoises {
    static final Noise TEMPERATURE = new SimplexNoise(new WorldgenRandom(new LegacyRandomSource(1234L)), true);
    static final Noise FROZEN_TEMPERATURE = frozen();
    static final Noise INFO = new SimplexNoise(new WorldgenRandom(new LegacyRandomSource(2345L)), true);

    private BiomeNoises() {}

    private static Noise frozen() {
        WorldgenRandom random = new WorldgenRandom(new LegacyRandomSource(3456L));
        return new Layers(new Noise[] {new SimplexNoise(random, true), new SimplexNoise(random, true), new SimplexNoise(random, true)},
            new double[] {1.0, 0.5, 0.25}, new float[] {0.14285715F, 0.2857143F, 0.5714286F});
    }

    private record Layers(Noise[] noises, double[] frequencies, float[] amplitudes) implements Noise {
        @Override
        public Interval range() {
            return null;
        }

        @Override
        public float get(double x, double y) {
            float sum = 0.0F;
            for (int i = 0; i < noises.length; i++) {
                sum += amplitudes[i] * noises[i].get(x * frequencies[i], y * frequencies[i]);
            }
            return sum;
        }

        @Override
        public float get(double x, double y, double z) {
            float sum = 0.0F;
            for (int i = 0; i < noises.length; i++) {
                sum += amplitudes[i] * noises[i].get(x * frequencies[i], y * frequencies[i], z * frequencies[i]);
            }
            return sum;
        }
    }
}
