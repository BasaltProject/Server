// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class VillagePlacements {
    public static final ResourceKey<PlacedFeature> PILE_HAY_VILLAGE = k("pile_hay");
    public static final ResourceKey<PlacedFeature> PILE_MELON_VILLAGE = k("pile_melon");
    public static final ResourceKey<PlacedFeature> PILE_SNOW_VILLAGE = k("pile_snow");
    public static final ResourceKey<PlacedFeature> PILE_ICE_VILLAGE = k("pile_ice");
    public static final ResourceKey<PlacedFeature> PILE_PUMPKIN_VILLAGE = k("pile_pumpkin");
    public static final ResourceKey<PlacedFeature> OAK_VILLAGE = k("oak");
    public static final ResourceKey<PlacedFeature> ACACIA_VILLAGE = k("acacia");
    public static final ResourceKey<PlacedFeature> SPRUCE_VILLAGE = k("spruce");
    public static final ResourceKey<PlacedFeature> PINE_VILLAGE = k("pine");
    public static final ResourceKey<PlacedFeature> PATCH_CACTUS_VILLAGE = k("patch_cactus");
    public static final ResourceKey<PlacedFeature> FLOWER_PLAIN_VILLAGE = k("flower_plain");
    public static final ResourceKey<PlacedFeature> PATCH_TAIGA_GRASS_VILLAGE = k("patch_taiga_grass");
    public static final ResourceKey<PlacedFeature> PATCH_BERRY_BUSH_VILLAGE = k("patch_berry_bush");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}
