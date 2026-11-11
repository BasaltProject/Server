// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;
import net.minecraft.util.Mth;
import net.minecraft.world.phys.Vec3;
public class GaussianSampler {
    private static final int kRadius = 2;
    private static final int kBreadth = 6;
    private static final double[] kKernel = {0.0, 1.0, 4.0, 6.0, 4.0, 1.0, 0.0};
    public static <V> void sample(Vec3 position, Sampler<V> sampler, Accumulator<V> accumulator) {
        Vec3 origin = position.subtract(0.5, 0.5, 0.5);
        int ix = Mth.floor(origin.x());
        int iy = Mth.floor(origin.y());
        int iz = Mth.floor(origin.z());
        double fx = origin.x() - ix;
        double fy = origin.y() - iy;
        double fz = origin.z() - iz;
        for (int dz = 0; dz < kBreadth; dz++) {
            double wz = Mth.lerp(fz, kKernel[dz + 1], kKernel[dz]);
            for (int dx = 0; dx < kBreadth; dx++) {
                double wx = Mth.lerp(fx, kKernel[dx + 1], kKernel[dx]);
                for (int dy = 0; dy < kBreadth; dy++) {
                    double wy = Mth.lerp(fy, kKernel[dy + 1], kKernel[dy]);
                    accumulator.accumulate(wx * wy * wz, sampler.get(ix - kRadius + dx, iy - kRadius + dy, iz - kRadius + dz));
                }
            }
        }
    }
    @FunctionalInterface
    public interface Accumulator<V> {
        void accumulate(double weight, V value);
    }
    @FunctionalInterface
    public interface Sampler<V> {
        V get(int x, int y, int z);
    }
}
