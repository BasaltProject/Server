// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.biome;
import net.minecraft.util.Mth;
import net.minecraft.util.ARGB;
import net.minecraft.resources.Identifier;
import net.minecraft.core.HolderGetter;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.levelgen.carver.WorldCarver;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class OverworldBiomes {
    protected static final int NORMAL_WATER_COLOR = 4159204;
    public static final int SWAMP_SKELETON_WEIGHT = 70;
    public static int calculateSkyColor(float temperature) {
        float temp = Mth.clamp(temperature / 3.0F, -1.0F, 1.0F);
        return ARGB.opaque(Mth.hsvToRgb(0.62222224F - temp * 0.05F, 0.5F + temp * 0.1F, 1.0F));
    }
    public static Biome oldGrowthTaiga(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean spruce) {
        return new Biome(Identifier.withDefaultNamespace("old_growth_taiga"));
    }
    public static Biome sparseJungle(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("sparse_jungle"));
    }
    public static Biome jungle(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("jungle"));
    }
    public static Biome bambooJungle(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("bamboo_jungle"));
    }
    public static Biome windsweptHills(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean moreTrees) {
        return new Biome(Identifier.withDefaultNamespace("windswept_hills"));
    }
    public static Biome desert(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("desert"));
    }
    public static Biome plains(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean sunflower, boolean snowy, boolean spikes) {
        return new Biome(Identifier.withDefaultNamespace("plains"));
    }
    public static Biome mushroomFields(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("mushroom_fields"));
    }
    public static Biome savanna(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean shattered, boolean plateau) {
        return new Biome(Identifier.withDefaultNamespace("savanna"));
    }
    public static Biome badlands(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean wooded) {
        return new Biome(Identifier.withDefaultNamespace("badlands"));
    }
    public static Biome coldOcean(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean deep) {
        return new Biome(Identifier.withDefaultNamespace("cold_ocean"));
    }
    public static Biome ocean(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean deep) {
        return new Biome(Identifier.withDefaultNamespace("ocean"));
    }
    public static Biome lukeWarmOcean(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean deep) {
        return new Biome(Identifier.withDefaultNamespace("luke_warm_ocean"));
    }
    public static Biome warmOcean(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("warm_ocean"));
    }
    public static Biome frozenOcean(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean deep) {
        return new Biome(Identifier.withDefaultNamespace("frozen_ocean"));
    }
    public static Biome forest(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean birch, boolean tall, boolean flower) {
        return new Biome(Identifier.withDefaultNamespace("forest"));
    }
    public static Biome taiga(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean snowy) {
        return new Biome(Identifier.withDefaultNamespace("taiga"));
    }
    public static Biome darkForest(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean isPaleGarden) {
        return new Biome(Identifier.withDefaultNamespace("dark_forest"));
    }
    public static Biome swamp(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("swamp"));
    }
    public static Biome mangroveSwamp(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("mangrove_swamp"));
    }
    public static Biome river(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean frozen) {
        return new Biome(Identifier.withDefaultNamespace("river"));
    }
    public static Biome beach(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean snowy, boolean stony) {
        return new Biome(Identifier.withDefaultNamespace("beach"));
    }
    public static Biome theVoid(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("the_void"));
    }
    public static Biome meadowOrCherryGrove(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers, boolean cherryGrove) {
        return new Biome(Identifier.withDefaultNamespace("meadow_or_cherry_grove"));
    }
    public static Biome dappledForest(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("dappled_forest"));
    }
    public static Biome frozenPeaks(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("frozen_peaks"));
    }
    public static Biome jaggedPeaks(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("jagged_peaks"));
    }
    public static Biome stonyPeaks(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("stony_peaks"));
    }
    public static Biome snowySlopes(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("snowy_slopes"));
    }
    public static Biome grove(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("grove"));
    }
    public static Biome sulfurCaves(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("sulfur_caves"));
    }
    public static Biome lushCaves(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("lush_caves"));
    }
    public static Biome dripstoneCaves(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("dripstone_caves"));
    }
    public static Biome deepDark(HolderGetter<PlacedFeature> placedFeatures, HolderGetter<WorldCarver> carvers) {
        return new Biome(Identifier.withDefaultNamespace("deep_dark"));
    }
}
