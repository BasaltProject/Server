// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import net.minecraft.util.RandomSource;

public abstract class GradientNoise implements Noise {
    protected static final Gradient[] GRADIENT = {
        new Gradient(1, 1, 0), new Gradient(-1, 1, 0), new Gradient(1, -1, 0), new Gradient(-1, -1, 0),
        new Gradient(1, 0, 1), new Gradient(-1, 0, 1), new Gradient(1, 0, -1), new Gradient(-1, 0, -1),
        new Gradient(0, 1, 1), new Gradient(0, -1, 1), new Gradient(0, 1, -1), new Gradient(0, -1, -1),
        new Gradient(1, 1, 0), new Gradient(0, -1, 1), new Gradient(-1, 1, 0), new Gradient(0, -1, -1)};
    private static final double kHalfWrap = Math.nextDown(1.6777216E7F);
    private static final double kWrap = 3.3554432E7F;
    protected final byte[] perms = new byte[256];
    protected final double offsetX;
    protected final double offsetY;
    protected final double offsetZ;

    protected GradientNoise(RandomSource random) {
        this(random, 256.0);
    }

    protected GradientNoise(RandomSource random, double noiseOffsetScale) {
        offsetX = random.nextDouble() * noiseOffsetScale;
        offsetY = random.nextDouble() * noiseOffsetScale;
        offsetZ = random.nextDouble() * noiseOffsetScale;
        for (int i = 0; i < 256; i++) {
            perms[i] = (byte) i;
        }
        for (int i = 0; i < 256; i++) {
            int swapWith = i + random.nextInt(256 - i);
            byte held = perms[i];
            perms[i] = perms[swapWith];
            perms[swapWith] = held;
        }
    }

    protected int permute(int x) {
        return perms[x & 255] & 255;
    }

    protected Gradient permuteToGrad(int x) {
        return GRADIENT[permute(x) & 15];
    }

    protected static float gradDot(int hash, float x, float y, float z) {
        return GRADIENT[hash & 15].dot(x, y, z);
    }

    protected static double wrap(double x) {
        if (x >= -kHalfWrap && x < kHalfWrap) {
            return x;
        }
        return x - Math.floor(x / kWrap + 0.5) * kWrap;
    }

    protected record Gradient(int x, int y, int z) {
        public double dot(double px, double py, double pz) {
            return x * px + y * py + z * pz;
        }

        public float dot(float px, float py, float pz) {
            return x * px + y * py + z * pz;
        }

        public float dotXz(float px, float pz) {
            return x * px + z * pz;
        }
    }
}
