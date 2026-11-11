// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.biome;
import net.minecraft.resources.Identifier;
import net.minecraft.core.HolderGetter;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.levelgen.carver.WorldCarver;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class EndBiomes {
    public static Biome endBarrens(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("end_barrens"));
    }
    public static Biome theEnd(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("the_end"));
    }
    public static Biome endMidlands(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("end_midlands"));
    }
    public static Biome endHighlands(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("end_highlands"));
    }
    public static Biome smallEndIslands(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("small_end_islands"));
    }
}
