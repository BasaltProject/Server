// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import net.minecraft.util.Interval;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;

public class PerlinNoise extends GradientNoise {
    public static final Interval RANGE = Interval.ofSymmetric(2.0F);
    public static final double STANDARD_DEVIATION = 0.2702247831245211;

    public PerlinNoise(RandomSource random) {
        super(random);
    }

    public Interval range() {
        return RANGE;
    }

    public float get(double x, double y) {
        return get(wrap(x), 0.0, wrap(y));
    }

    public float get(double inX, double inY, double inZ) {
        double x = wrap(inX) + offsetX;
        double y = wrap(inY) + offsetY;
        double z = wrap(inZ) + offsetZ;
        int fx = Mth.floor(x);
        int fy = Mth.floor(y);
        int fz = Mth.floor(z);
        float ry = (float) (y - fy);
        return sampleAndLerp(fx, fy, fz, (float) (x - fx), ry, (float) (z - fz), ry);
    }

    public float noiseWithDerivative(double inX, double inY, double inZ, float[] derivativeOut) {
        double x = wrap(inX) + offsetX;
        double y = wrap(inY) + offsetY;
        double z = wrap(inZ) + offsetZ;
        int fx = Mth.floor(x);
        int fy = Mth.floor(y);
        int fz = Mth.floor(z);
        return sampleWithDerivative(fx, fy, fz, (float) (x - fx), (float) (y - fy), (float) (z - fz), derivativeOut);
    }

    protected float sampleAndLerp(int x, int y, int z, float rx, float ry, float rz, float originalRy) {
        int x0 = permute(x);
        int x1 = permute(x + 1);
        int a = permute(x0 + y);
        int b = permute(x0 + y + 1);
        int c = permute(x1 + y);
        int d = permute(x1 + y + 1);
        return Mth.lerp3(Mth.smoothstep(rx), Mth.smoothstep(originalRy), Mth.smoothstep(rz),
            gradDot(permute(a + z), rx, ry, rz),
            gradDot(permute(c + z), rx - 1.0F, ry, rz),
            gradDot(permute(b + z), rx, ry - 1.0F, rz),
            gradDot(permute(d + z), rx - 1.0F, ry - 1.0F, rz),
            gradDot(permute(a + z + 1), rx, ry, rz - 1.0F),
            gradDot(permute(c + z + 1), rx - 1.0F, ry, rz - 1.0F),
            gradDot(permute(b + z + 1), rx, ry - 1.0F, rz - 1.0F),
            gradDot(permute(d + z + 1), rx - 1.0F, ry - 1.0F, rz - 1.0F));
    }

    protected float yOffsetForVolume(double originalY, double relativeY) {
        return (float) relativeY;
    }

    public void addToVolume(DensityBuffer buffer, DensityVolume volume, double xzScale, double yScale, float amplitude) {
        float[] dxz = new float[8];
        float[] gy = new float[8];
        int index = 0;
        for (int iz = 0; iz < volume.sizeZ(); iz++) {
            double z = wrap(volume.blockZ(iz) * xzScale) + offsetZ;
            int fz = Mth.floor(z);
            float rz = (float) (z - fz);
            float alphaZ = Mth.smoothstep(rz);
            for (int ix = 0; ix < volume.sizeX(); ix++) {
                double x = wrap(volume.blockX(ix) * xzScale) + offsetX;
                int fx = Mth.floor(x);
                float rx = (float) (x - fx);
                int x0 = permute(fx);
                int x1 = permute(fx + 1);
                float alphaX = Mth.smoothstep(rx);
                int lastFy = Integer.MIN_VALUE;
                for (int iy = 0; iy < volume.sizeY(); iy++) {
                    double originalY = volume.blockY(iy) * yScale;
                    double y = wrap(originalY) + offsetY;
                    int fy = Mth.floor(y);
                    double ry = y - fy;
                    float alphaY = Mth.smoothstep((float) ry);
                    if (lastFy != fy) {
                        int[] hashes = {permute(x0 + fy), permute(x1 + fy), permute(x0 + fy + 1), permute(x1 + fy + 1)};
                        for (int corner = 0; corner < 8; corner++) {
                            int base = hashes[corner & 3];
                            int zz = (corner >> 2) & 1;
                            Gradient g = permuteToGrad(base + fz + zz);
                            dxz[corner] = g.dotXz((corner & 1) == 0 ? rx : rx - 1.0F, zz == 0 ? rz : rz - 1.0F);
                            gy[corner] = (float) g.y();
                        }
                        lastFy = fy;
                    }
                    float fy0 = yOffsetForVolume(originalY, ry);
                    float fy1 = fy0 - 1.0F;
                    buffer.addTo(index, amplitude * Mth.lerp3(alphaX, alphaY, alphaZ,
                        dxz[0] + gy[0] * fy0, dxz[1] + gy[1] * fy0, dxz[2] + gy[2] * fy1, dxz[3] + gy[3] * fy1,
                        dxz[4] + gy[4] * fy0, dxz[5] + gy[5] * fy0, dxz[6] + gy[6] * fy1, dxz[7] + gy[7] * fy1));
                    index++;
                }
            }
        }
    }

    private float sampleWithDerivative(int x, int y, int z, float xr, float yr, float zr, float[] out) {
        int x0 = permute(x);
        int x1 = permute(x + 1);
        int[] hashes = {permute(x0 + y), permute(x1 + y), permute(x0 + y + 1), permute(x1 + y + 1)};
        Gradient[] g = new Gradient[8];
        float[] d = new float[8];
        for (int corner = 0; corner < 8; corner++) {
            int zz = (corner >> 2) & 1;
            g[corner] = permuteToGrad(hashes[corner & 3] + z + zz);
            d[corner] = g[corner].dot((corner & 1) == 0 ? xr : xr - 1.0F, (corner & 2) == 0 ? yr : yr - 1.0F,
                zz == 0 ? zr : zr - 1.0F);
        }
        float xa = Mth.smoothstep(xr);
        float ya = Mth.smoothstep(yr);
        float za = Mth.smoothstep(zr);
        float g1x = Mth.lerp3(xa, ya, za, (float) g[0].x(), (float) g[1].x(), (float) g[2].x(), (float) g[3].x(),
            (float) g[4].x(), (float) g[5].x(), (float) g[6].x(), (float) g[7].x());
        float g1y = Mth.lerp3(xa, ya, za, (float) g[0].y(), (float) g[1].y(), (float) g[2].y(), (float) g[3].y(),
            (float) g[4].y(), (float) g[5].y(), (float) g[6].y(), (float) g[7].y());
        float g1z = Mth.lerp3(xa, ya, za, (float) g[0].z(), (float) g[1].z(), (float) g[2].z(), (float) g[3].z(),
            (float) g[4].z(), (float) g[5].z(), (float) g[6].z(), (float) g[7].z());
        float s2x = Mth.lerp2(ya, za, d[1] - d[0], d[3] - d[2], d[5] - d[4], d[7] - d[6]);
        float s2y = Mth.lerp2(za, xa, d[2] - d[0], d[6] - d[4], d[3] - d[1], d[7] - d[5]);
        float s2z = Mth.lerp2(xa, ya, d[4] - d[0], d[5] - d[1], d[6] - d[2], d[7] - d[3]);
        out[0] += g1x + Mth.smoothstepDerivative(xr) * s2x;
        out[1] += g1y + Mth.smoothstepDerivative(yr) * s2y;
        out[2] += g1z + Mth.smoothstepDerivative(zr) * s2z;
        return Mth.lerp3(xa, ya, za, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
    }

    public void parityConfigString(StringBuilder sb) {
        NoiseUtils.parityNoiseOctaveConfigString(sb, offsetX, offsetY, offsetZ, perms);
    }
}
