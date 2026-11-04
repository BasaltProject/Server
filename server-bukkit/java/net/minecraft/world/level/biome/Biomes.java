// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import net.minecraft.resources.ResourceKey;
public abstract class Biomes {
    @SuppressWarnings({"unchecked", "rawtypes"})
    private static <T> ResourceKey<T> key(String name) {
        return (ResourceKey) ResourceKey.create(net.minecraft.core.registries.Registries.BIOME,
            net.minecraft.resources.Identifier.withDefaultNamespace(name));
    }

    public static final ResourceKey<Biome> THE_VOID = key("the_void");
    public static final ResourceKey<Biome> PLAINS = key("plains");
    public static final ResourceKey<Biome> SUNFLOWER_PLAINS = key("sunflower_plains");
    public static final ResourceKey<Biome> SNOWY_PLAINS = key("snowy_plains");
    public static final ResourceKey<Biome> ICE_SPIKES = key("ice_spikes");
    public static final ResourceKey<Biome> DESERT = key("desert");
    public static final ResourceKey<Biome> SWAMP = key("swamp");
    public static final ResourceKey<Biome> MANGROVE_SWAMP = key("mangrove_swamp");
    public static final ResourceKey<Biome> FOREST = key("forest");
    public static final ResourceKey<Biome> FLOWER_FOREST = key("flower_forest");
    public static final ResourceKey<Biome> BIRCH_FOREST = key("birch_forest");
    public static final ResourceKey<Biome> DAPPLED_FOREST = key("dappled_forest");
    public static final ResourceKey<Biome> DARK_FOREST = key("dark_forest");
    public static final ResourceKey<Biome> PALE_GARDEN = key("pale_garden");
    public static final ResourceKey<Biome> OLD_GROWTH_BIRCH_FOREST = key("old_growth_birch_forest");
    public static final ResourceKey<Biome> OLD_GROWTH_PINE_TAIGA = key("old_growth_pine_taiga");
    public static final ResourceKey<Biome> OLD_GROWTH_SPRUCE_TAIGA = key("old_growth_spruce_taiga");
    public static final ResourceKey<Biome> TAIGA = key("taiga");
    public static final ResourceKey<Biome> SNOWY_TAIGA = key("snowy_taiga");
    public static final ResourceKey<Biome> SAVANNA = key("savanna");
    public static final ResourceKey<Biome> SAVANNA_PLATEAU = key("savanna_plateau");
    public static final ResourceKey<Biome> WINDSWEPT_HILLS = key("windswept_hills");
    public static final ResourceKey<Biome> WINDSWEPT_GRAVELLY_HILLS = key("windswept_gravelly_hills");
    public static final ResourceKey<Biome> WINDSWEPT_FOREST = key("windswept_forest");
    public static final ResourceKey<Biome> WINDSWEPT_SAVANNA = key("windswept_savanna");
    public static final ResourceKey<Biome> JUNGLE = key("jungle");
    public static final ResourceKey<Biome> SPARSE_JUNGLE = key("sparse_jungle");
    public static final ResourceKey<Biome> BAMBOO_JUNGLE = key("bamboo_jungle");
    public static final ResourceKey<Biome> BADLANDS = key("badlands");
    public static final ResourceKey<Biome> ERODED_BADLANDS = key("eroded_badlands");
    public static final ResourceKey<Biome> WOODED_BADLANDS = key("wooded_badlands");
    public static final ResourceKey<Biome> MEADOW = key("meadow");
    public static final ResourceKey<Biome> CHERRY_GROVE = key("cherry_grove");
    public static final ResourceKey<Biome> GROVE = key("grove");
    public static final ResourceKey<Biome> SNOWY_SLOPES = key("snowy_slopes");
    public static final ResourceKey<Biome> FROZEN_PEAKS = key("frozen_peaks");
    public static final ResourceKey<Biome> JAGGED_PEAKS = key("jagged_peaks");
    public static final ResourceKey<Biome> STONY_PEAKS = key("stony_peaks");
    public static final ResourceKey<Biome> RIVER = key("river");
    public static final ResourceKey<Biome> FROZEN_RIVER = key("frozen_river");
    public static final ResourceKey<Biome> BEACH = key("beach");
    public static final ResourceKey<Biome> SNOWY_BEACH = key("snowy_beach");
    public static final ResourceKey<Biome> STONY_SHORE = key("stony_shore");
    public static final ResourceKey<Biome> WARM_OCEAN = key("warm_ocean");
    public static final ResourceKey<Biome> LUKEWARM_OCEAN = key("lukewarm_ocean");
    public static final ResourceKey<Biome> DEEP_LUKEWARM_OCEAN = key("deep_lukewarm_ocean");
    public static final ResourceKey<Biome> OCEAN = key("ocean");
    public static final ResourceKey<Biome> DEEP_OCEAN = key("deep_ocean");
    public static final ResourceKey<Biome> COLD_OCEAN = key("cold_ocean");
    public static final ResourceKey<Biome> DEEP_COLD_OCEAN = key("deep_cold_ocean");
    public static final ResourceKey<Biome> FROZEN_OCEAN = key("frozen_ocean");
    public static final ResourceKey<Biome> DEEP_FROZEN_OCEAN = key("deep_frozen_ocean");
    public static final ResourceKey<Biome> MUSHROOM_FIELDS = key("mushroom_fields");
    public static final ResourceKey<Biome> DRIPSTONE_CAVES = key("dripstone_caves");
    public static final ResourceKey<Biome> LUSH_CAVES = key("lush_caves");
    public static final ResourceKey<Biome> DEEP_DARK = key("deep_dark");
    public static final ResourceKey<Biome> SULFUR_CAVES = key("sulfur_caves");
    public static final ResourceKey<Biome> NETHER_WASTES = key("nether_wastes");
    public static final ResourceKey<Biome> WARPED_FOREST = key("warped_forest");
    public static final ResourceKey<Biome> CRIMSON_FOREST = key("crimson_forest");
    public static final ResourceKey<Biome> SOUL_SAND_VALLEY = key("soul_sand_valley");
    public static final ResourceKey<Biome> BASALT_DELTAS = key("basalt_deltas");
    public static final ResourceKey<Biome> THE_END = key("the_end");
    public static final ResourceKey<Biome> END_HIGHLANDS = key("end_highlands");
    public static final ResourceKey<Biome> END_MIDLANDS = key("end_midlands");
    public static final ResourceKey<Biome> SMALL_END_ISLANDS = key("small_end_islands");
    public static final ResourceKey<Biome> END_BARRENS = key("end_barrens");
}
