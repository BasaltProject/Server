// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public class EndPlacements {
    public static final ResourceKey<PlacedFeature> END_PLATFORM = k("end_platform");
    public static final ResourceKey<PlacedFeature> END_SPIKE = k("end_spike");
    public static final ResourceKey<PlacedFeature> END_GATEWAY_RETURN = k("end_gateway_return");
    public static final ResourceKey<PlacedFeature> CHORUS_PLANT = k("chorus_plant");
    public static final ResourceKey<PlacedFeature> END_ISLAND_DECORATED = k("end_island_decorated");
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }

    private static ResourceKey<PlacedFeature> k(String n) {
        return PlacementUtils.createKey(n);
    }
}
