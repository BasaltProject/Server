// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import java.util.Set;
import net.minecraft.core.Holder;
public interface BiomeResolver {
    default Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeResolver#getNoiseBiome");
    }
    default Set<Holder<Biome>> getBiomesWithin(int x, int y, int z, int radius) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeResolver#getBiomesWithin");
    }
}
