// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import net.minecraft.util.Interval;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;

public class SimplexNoise extends GradientNoise {
    public static final Interval RANGE = Interval.ofSymmetric(2.0F);
    public static final double STANDARD_DEVIATION = 0.42544;
    private static final double kSkew2 = 0.5 * (Math.sqrt(3.0) - 1.0);
    private static final double kUnskew2 = (3.0 - Math.sqrt(3.0)) / 6.0;
    private static final double kSkew3 = 1.0 / 3.0;
    private static final double kUnskew3 = 1.0 / 6.0;

    public SimplexNoise(RandomSource random) {
        super(random);
    }

    public SimplexNoise(RandomSource random, boolean discardNoiseOffset) {
        super(random, discardNoiseOffset ? 0.0 : 256.0);
    }

    public Interval range() {
        return RANGE;
    }

    private double contribution(int gradient, double dx, double dy, double dz, double radius) {
        double falloff = radius - dx * dx - dy * dy - dz * dz;
        if (falloff < 0.0) {
            return 0.0;
        }
        falloff *= falloff;
        return falloff * falloff * GRADIENT[gradient].dot(dx, dy, dz);
    }

    public float get(double xin, double yin) {
        double px = xin + offsetX;
        double py = yin + offsetY;
        double skew = (px + py) * kSkew2;
        int cellX = Mth.floor(px + skew);
        int cellY = Mth.floor(py + skew);
        double origin = (cellX + cellY) * kUnskew2;
        double x0 = px - (cellX - origin);
        double y0 = py - (cellY - origin);
        int stepX = x0 > y0 ? 1 : 0;
        int stepY = 1 - stepX;
        double x1 = x0 - stepX + kUnskew2;
        double y1 = y0 - stepY + kUnskew2;
        double x2 = x0 - 1.0 + 2.0 * kUnskew2;
        double y2 = y0 - 1.0 + 2.0 * kUnskew2;
        int ii = cellX & 255;
        int jj = cellY & 255;
        int g0 = permute(ii + permute(jj)) % 12;
        int g1 = permute(ii + stepX + permute(jj + stepY)) % 12;
        int g2 = permute(ii + 1 + permute(jj + 1)) % 12;
        double sum = contribution(g0, x0, y0, 0.0, 0.5) + contribution(g1, x1, y1, 0.0, 0.5) + contribution(g2, x2, y2, 0.0, 0.5);
        return (float) (70.0 * sum);
    }

    public float get(double xin, double yin, double zin) {
        double px = xin + offsetX;
        double py = yin + offsetY;
        double pz = zin + offsetZ;
        double skew = (px + py + pz) * kSkew3;
        int cx = Mth.floor(px + skew);
        int cy = Mth.floor(py + skew);
        int cz = Mth.floor(pz + skew);
        double origin = (cx + cy + cz) * kUnskew3;
        double x0 = px - (cx - origin);
        double y0 = py - (cy - origin);
        double z0 = pz - (cz - origin);
        int[] second = new int[3];
        int[] third = new int[3];
        int first = x0 >= y0 ? (y0 >= z0 ? 0 : (x0 >= z0 ? 0 : 2)) : (y0 < z0 ? 2 : 1);
        int last = x0 >= y0 ? (y0 >= z0 ? 2 : (x0 >= z0 ? 1 : 0)) : (y0 < z0 ? 0 : (x0 < z0 ? 0 : 2));
        second[first] = 1;
        third[first] = 1;
        third[3 - first - last] = 1;
        double x1 = x0 - second[0] + kUnskew3;
        double y1 = y0 - second[1] + kUnskew3;
        double z1 = z0 - second[2] + kUnskew3;
        double x2 = x0 - third[0] + 2.0 * kUnskew3;
        double y2 = y0 - third[1] + 2.0 * kUnskew3;
        double z2 = z0 - third[2] + 2.0 * kUnskew3;
        double x3 = x0 - 1.0 + 3.0 * kUnskew3;
        double y3 = y0 - 1.0 + 3.0 * kUnskew3;
        double z3 = z0 - 1.0 + 3.0 * kUnskew3;
        int ii = cx & 255;
        int jj = cy & 255;
        int kk = cz & 255;
        int g0 = permute(ii + permute(jj + permute(kk))) % 12;
        int g1 = permute(ii + second[0] + permute(jj + second[1] + permute(kk + second[2]))) % 12;
        int g2 = permute(ii + third[0] + permute(jj + third[1] + permute(kk + third[2]))) % 12;
        int g3 = permute(ii + 1 + permute(jj + 1 + permute(kk + 1))) % 12;
        double sum = contribution(g0, x0, y0, z0, 0.6) + contribution(g1, x1, y1, z1, 0.6)
            + contribution(g2, x2, y2, z2, 0.6) + contribution(g3, x3, y3, z3, 0.6);
        return (float) (32.0 * sum);
    }
}
