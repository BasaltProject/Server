// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.data.worldgen.placement;
import net.minecraft.resources.Identifier;
import net.minecraft.core.registries.Registries;
import java.util.List;
import net.minecraft.core.Holder;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.levelgen.feature.Feature;
import net.minecraft.world.level.levelgen.placement.BlockPredicateFilter;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.placement.PlacementFilter;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;
public class PlacementUtils {
    public static final PlacementModifier HEIGHTMAP = net.minecraft.world.level.levelgen.placement.HeightmapPlacement.onHeightmap(net.minecraft.world.level.levelgen.Heightmap.Types.MOTION_BLOCKING);
    public static final PlacementModifier HEIGHTMAP_NO_LEAVES = net.minecraft.world.level.levelgen.placement.HeightmapPlacement.onHeightmap(net.minecraft.world.level.levelgen.Heightmap.Types.MOTION_BLOCKING_NO_LEAVES);
    public static final PlacementModifier HEIGHTMAP_TOP_SOLID = net.minecraft.world.level.levelgen.placement.HeightmapPlacement.onHeightmap(net.minecraft.world.level.levelgen.Heightmap.Types.OCEAN_FLOOR_WG);
    public static final PlacementModifier HEIGHTMAP_WORLD_SURFACE = net.minecraft.world.level.levelgen.placement.HeightmapPlacement.onHeightmap(net.minecraft.world.level.levelgen.Heightmap.Types.WORLD_SURFACE_WG);
    public static final PlacementModifier HEIGHTMAP_OCEAN_FLOOR = net.minecraft.world.level.levelgen.placement.HeightmapPlacement.onHeightmap(net.minecraft.world.level.levelgen.Heightmap.Types.OCEAN_FLOOR);
    public static final PlacementModifier FULL_RANGE = net.minecraft.world.level.levelgen.placement.HeightRangePlacement.uniform(net.minecraft.world.level.levelgen.VerticalAnchor.bottom(), net.minecraft.world.level.levelgen.VerticalAnchor.top());
    public static final PlacementModifier RANGE_10_10 = net.minecraft.world.level.levelgen.placement.HeightRangePlacement.uniform(net.minecraft.world.level.levelgen.VerticalAnchor.aboveBottom(10), net.minecraft.world.level.levelgen.VerticalAnchor.belowTop(10));
    public static final PlacementModifier RANGE_8_8 = net.minecraft.world.level.levelgen.placement.HeightRangePlacement.uniform(net.minecraft.world.level.levelgen.VerticalAnchor.aboveBottom(8), net.minecraft.world.level.levelgen.VerticalAnchor.belowTop(8));
    public static final PlacementModifier RANGE_4_4 = net.minecraft.world.level.levelgen.placement.HeightRangePlacement.uniform(net.minecraft.world.level.levelgen.VerticalAnchor.aboveBottom(4), net.minecraft.world.level.levelgen.VerticalAnchor.belowTop(4));
    public static final PlacementModifier RANGE_BOTTOM_TO_MAX_TERRAIN_HEIGHT = net.minecraft.world.level.levelgen.placement.HeightRangePlacement.uniform(net.minecraft.world.level.levelgen.VerticalAnchor.bottom(), net.minecraft.world.level.levelgen.VerticalAnchor.absolute(256));
    public static void bootstrap(BootstrapContext<PlacedFeature> context) {
    }
    public static ResourceKey<PlacedFeature> createKey(String name) {
        return ResourceKey.create((ResourceKey) Registries.PLACED_FEATURE, Identifier.withDefaultNamespace(name));
    }
    public static void register(BootstrapContext<PlacedFeature> context, ResourceKey<PlacedFeature> id, Holder<Feature> feature, List<PlacementModifier> placementModifiers) {
    }
    public static void register(BootstrapContext<PlacedFeature> context, ResourceKey<PlacedFeature> id, Holder<Feature> feature, PlacementModifier... placementModifiers) {
    }
    public static PlacementModifier countExtra(int count, float chance, int extra) {
        return null;
    }
    public static PlacementFilter isEmpty() {
        return null;
    }
    public static BlockPredicateFilter filteredByBlockSurvival(Block block) {
        return null;
    }
    public static Holder<PlacedFeature> inlinePlaced(Holder<Feature> feature, List<PlacementModifier> placementModifiers) {
        return Holder.direct(new PlacedFeature(Holder.direct(null), List.of()));
    }
    public static Holder<PlacedFeature> inlinePlaced(Holder<Feature> feature, PlacementModifier... placementModifiers) {
        return Holder.direct(new PlacedFeature(Holder.direct(null), List.of()));
    }
    public static Holder<PlacedFeature> inlinePlaced(Feature feature, PlacementModifier... placementModifiers) {
        return Holder.direct(new PlacedFeature(Holder.direct(null), List.of()));
    }
    public static Holder<PlacedFeature> inlinePlaced(Feature feature, List<PlacementModifier> placementModifiers) {
        return Holder.direct(new PlacedFeature(Holder.direct(null), List.of()));
    }
}
