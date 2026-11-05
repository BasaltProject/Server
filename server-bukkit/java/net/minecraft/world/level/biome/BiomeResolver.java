// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import java.util.Set;
import net.minecraft.core.Holder;
public interface BiomeResolver {
    Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ);
    default Set<Holder<Biome>> getBiomesWithin(int x, int y, int z, int radius) {
        Set<Holder<Biome>> found = new java.util.LinkedHashSet<>();
        for (int qx = (x - radius) >> 2; qx <= (x + radius) >> 2; qx++) {
            for (int qy = (y - radius) >> 2; qy <= (y + radius) >> 2; qy++) {
                for (int qz = (z - radius) >> 2; qz <= (z + radius) >> 2; qz++) {
                    found.add(getNoiseBiome(qx, qy, qz));
                }
            }
        }
        return found;
    }
}
