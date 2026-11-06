// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.synth;

import net.minecraft.util.Interval;
import net.minecraft.world.level.levelgen.densityfunction.DensityBuffer;
import net.minecraft.world.level.levelgen.densityfunction.DensityVolume;

public interface Noise {
    Interval range();

    float get(double x, double y);

    float get(double x, double y, double z);

    default void addToVolume(DensityBuffer buffer, DensityVolume volume, double xzScale, double yScale, float amplitude) {
        int index = 0;
        for (int iz = 0; iz < volume.sizeZ(); iz++) {
            double z = volume.blockZ(iz) * xzScale;
            for (int ix = 0; ix < volume.sizeX(); ix++) {
                double x = volume.blockX(ix) * xzScale;
                for (int iy = 0; iy < volume.sizeY(); iy++) {
                    buffer.addTo(index++, amplitude * get(x, volume.blockY(iy) * yScale, z));
                }
            }
        }
    }
}
