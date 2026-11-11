// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class AquaticPlacements {
    public static final ResourceKey<PlacedFeature> SEAGRASS_WARM = k("seagrass_warm");
    public static final ResourceKey<PlacedFeature> SEAGRASS_NORMAL = k("seagrass_normal");
    public static final ResourceKey<PlacedFeature> SEAGRASS_COLD = k("seagrass_cold");
    public static final ResourceKey<PlacedFeature> SEAGRASS_RIVER = k("seagrass_river");
    public static final ResourceKey<PlacedFeature> SEAGRASS_SWAMP = k("seagrass_swamp");
    public static final ResourceKey<PlacedFeature> SEAGRASS_DEEP_WARM = k("seagrass_deep_warm");
    public static final ResourceKey<PlacedFeature> SEAGRASS_DEEP = k("seagrass_deep");
    public static final ResourceKey<PlacedFeature> SEAGRASS_DEEP_COLD = k("seagrass_deep_cold");
    public static final ResourceKey<PlacedFeature> SEA_PICKLE = k("sea_pickle");
    public static final ResourceKey<PlacedFeature> KELP_COLD = k("kelp_cold");
    public static final ResourceKey<PlacedFeature> KELP_WARM = k("kelp_warm");
    public static final ResourceKey<PlacedFeature> WARM_OCEAN_VEGETATION = k("warm_ocean_vegetation");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}
