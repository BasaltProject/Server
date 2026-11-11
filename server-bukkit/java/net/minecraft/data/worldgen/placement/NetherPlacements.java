// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import java.util.List;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;
public class NetherPlacements {
    public static final ResourceKey<PlacedFeature> DELTA = k("delta");
    public static final ResourceKey<PlacedFeature> SMALL_BASALT_COLUMNS = k("small_basalt_columns");
    public static final ResourceKey<PlacedFeature> LARGE_BASALT_COLUMNS = k("large_basalt_columns");
    public static final ResourceKey<PlacedFeature> BASALT_BLOBS = k("basalt_blobs");
    public static final ResourceKey<PlacedFeature> BLACKSTONE_BLOBS = k("blackstone_blobs");
    public static final ResourceKey<PlacedFeature> GLOWSTONE_EXTRA = k("glowstone_extra");
    public static final ResourceKey<PlacedFeature> GLOWSTONE = k("glowstone");
    public static final ResourceKey<PlacedFeature> CRIMSON_FOREST_VEGETATION = k("crimson_forest_vegetation");
    public static final ResourceKey<PlacedFeature> WARPED_FOREST_VEGETATION = k("warped_forest_vegetation");
    public static final ResourceKey<PlacedFeature> NETHER_SPROUTS = k("nether_sprouts");
    public static final ResourceKey<PlacedFeature> TWISTING_VINES = k("twisting_vines");
    public static final ResourceKey<PlacedFeature> WEEPING_VINES = k("weeping_vines");
    public static final ResourceKey<PlacedFeature> PATCH_CRIMSON_ROOTS = k("patch_crimson_roots");
    public static final ResourceKey<PlacedFeature> BASALT_PILLAR = k("basalt_pillar");
    public static final ResourceKey<PlacedFeature> SPRING_DELTA = k("spring_delta");
    public static final ResourceKey<PlacedFeature> SPRING_CLOSED = k("spring_closed");
    public static final ResourceKey<PlacedFeature> SPRING_CLOSED_DOUBLE = k("spring_closed_double");
    public static final ResourceKey<PlacedFeature> SPRING_OPEN = k("spring_open");
    public static final ResourceKey<PlacedFeature> PATCH_SOUL_FIRE = k("patch_soul_fire");
    public static final ResourceKey<PlacedFeature> PATCH_FIRE = k("patch_fire");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }
    public static List<PlacementModifier> spreadTwistingVines(int spreadWidth, int spreadHeight) {
        return List.of();
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}
