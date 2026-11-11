// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import net.minecraft.util.Interval;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

@Deprecated
public class SmearedPerlinNoise extends PerlinNoise {
    private static final float kShiftUpEpsilon = 1.0E-7F;
    private final double fudgeYScale;

    public SmearedPerlinNoise(RandomSource random, double fudgeYScale) {
        super(random);
        this.fudgeYScale = fudgeYScale;
    }

    public static Interval range(double fudgeYScale) {
        return Interval.ofSymmetric((float) (Math.abs(fudgeYScale) + 2.0));
    }

    public Interval range() {
        return range(fudgeYScale);
    }

    public float get(double inX, double inY, double inZ) {
        double x = wrap(inX) + offsetX;
        double y = wrap(inY) + offsetY;
        double z = wrap(inZ) + offsetZ;
        int fx = Mth.floor(x);
        int fy = Mth.floor(y);
        int fz = Mth.floor(z);
        double ry = y - fy;
        return sampleAndLerp(fx, fy, fz, (float) (x - fx), yOffsetForVolume(inY, ry), (float) (z - fz), (float) ry);
    }

    protected float yOffsetForVolume(double originalY, double relativeY) {
        return (float) (relativeY - computeFudgeY(originalY, relativeY));
    }

    private double computeFudgeY(double originalY, double relativeY) {
        double limit = originalY >= 0.0 && originalY < relativeY ? originalY : relativeY;
        return Mth.floor(limit / fudgeYScale + kShiftUpEpsilon) * fudgeYScale;
    }
}
