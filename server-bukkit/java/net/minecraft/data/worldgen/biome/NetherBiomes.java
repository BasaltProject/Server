// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.biome;
import net.minecraft.resources.Identifier;
import net.minecraft.core.HolderGetter;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.levelgen.carver.WorldCarver;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class NetherBiomes {
    public static Biome netherWastes(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("nether_wastes"));
    }
    public static Biome soulSandValley(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("soul_sand_valley"));
    }
    public static Biome basaltDeltas(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("basalt_deltas"));
    }
    public static Biome crimsonForest(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("crimson_forest"));
    }
    public static Biome warpedForest(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("warped_forest"));
    }
}
