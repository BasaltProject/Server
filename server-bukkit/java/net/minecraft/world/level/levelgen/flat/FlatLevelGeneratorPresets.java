// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.flat;

import java.util.List;
import java.util.Optional;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.item.Items;
import net.minecraft.world.level.ItemLike;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.Biomes;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.structure.BuiltinStructureSets;
import net.minecraft.world.level.levelgen.structure.StructureSet;

public class FlatLevelGeneratorPresets {
    public static final ResourceKey<FlatLevelGeneratorPreset> CLASSIC_FLAT = key("classic_flat");
    public static final ResourceKey<FlatLevelGeneratorPreset> TUNNELERS_DREAM = key("tunnelers_dream");
    public static final ResourceKey<FlatLevelGeneratorPreset> WATER_WORLD = key("water_world");
    public static final ResourceKey<FlatLevelGeneratorPreset> OVERWORLD = key("overworld");
    public static final ResourceKey<FlatLevelGeneratorPreset> SNOWY_KINGDOM = key("snowy_kingdom");
    public static final ResourceKey<FlatLevelGeneratorPreset> BOTTOMLESS_PIT = key("bottomless_pit");
    public static final ResourceKey<FlatLevelGeneratorPreset> DESERT = key("desert");
    public static final ResourceKey<FlatLevelGeneratorPreset> REDSTONE_READY = key("redstone_ready");
    public static final ResourceKey<FlatLevelGeneratorPreset> TEST_WORLD = key("test_world");
    public static final ResourceKey<FlatLevelGeneratorPreset> THE_VOID = key("the_void");

    public static void bootstrap(BootstrapContext<FlatLevelGeneratorPreset> context) {
        new Bootstrap(context).run();
    }

    private static ResourceKey<FlatLevelGeneratorPreset> key(String name) {
        return ResourceKey.create(Registries.FLAT_LEVEL_GENERATOR_PRESET, Identifier.withDefaultNamespace(name));
    }

    static class Bootstrap {
        private final BootstrapContext<FlatLevelGeneratorPreset> context;

        private Bootstrap(BootstrapContext<FlatLevelGeneratorPreset> context) {
            this.context = context;
        }

        @SuppressWarnings("unchecked")
        private static <T> HolderGetter<T> typed(HolderGetter<?> getter) {
            return (HolderGetter<T>) getter;
        }

        private void add(ResourceKey<FlatLevelGeneratorPreset> key, ItemLike icon, ResourceKey<Biome> biome,
            List<ResourceKey<StructureSet>> structures, boolean decoration, boolean addLakes, FlatLayerInfo... layers) {
            HolderGetter<StructureSet> structureSets = typed(context.lookup(Registries.STRUCTURE_SET));
            HolderGetter<PlacedFeature> placedFeatures = typed(context.lookup(Registries.PLACED_FEATURE));
            HolderGetter<Biome> biomes = typed(context.lookup(Registries.BIOME));
            HolderSet<StructureSet> holders = HolderSet.direct(structures.stream().map(structureSets::getOrThrow).toList());
            FlatLevelGeneratorSettings settings = new FlatLevelGeneratorSettings(Optional.of(holders),
                biomes.getOrThrow(biome), FlatLevelGeneratorSettings.createLakesList(placedFeatures));
            if (decoration) {
                settings.setDecoration();
            }
            if (addLakes) {
                settings.setAddLakes();
            }
            for (int i = layers.length - 1; i >= 0; i--) {
                settings.getLayersInfo().add(layers[i]);
            }
            context.register(key, new FlatLevelGeneratorPreset(icon.asItem().builtInRegistryHolder(), settings));
        }

        public void run() {
        add(FlatLevelGeneratorPresets.CLASSIC_FLAT, Blocks.GRASS_BLOCK, Biomes.PLAINS, List.of(BuiltinStructureSets.VILLAGES),
            false, false, new FlatLayerInfo(1, Blocks.GRASS_BLOCK), new FlatLayerInfo(2, Blocks.DIRT), new FlatLayerInfo(1, Blocks.BEDROCK));
        add(FlatLevelGeneratorPresets.TUNNELERS_DREAM, Blocks.STONE, Biomes.WINDSWEPT_HILLS, List.of(BuiltinStructureSets.MINESHAFTS, BuiltinStructureSets.STRONGHOLDS),
            true, false, new FlatLayerInfo(1, Blocks.GRASS_BLOCK), new FlatLayerInfo(5, Blocks.DIRT), new FlatLayerInfo(230, Blocks.STONE), new FlatLayerInfo(1, Blocks.BEDROCK));
        add(FlatLevelGeneratorPresets.WATER_WORLD, Items.WATER_BUCKET, Biomes.DEEP_OCEAN, List.of(BuiltinStructureSets.OCEAN_RUINS, BuiltinStructureSets.SHIPWRECKS, BuiltinStructureSets.OCEAN_MONUMENTS),
            false, false, new FlatLayerInfo(90, Blocks.WATER), new FlatLayerInfo(5, Blocks.GRAVEL), new FlatLayerInfo(5, Blocks.DIRT), new FlatLayerInfo(5, Blocks.STONE), new FlatLayerInfo(64, Blocks.DEEPSLATE), new FlatLayerInfo(1, Blocks.BEDROCK));
        add(FlatLevelGeneratorPresets.OVERWORLD, Blocks.SHORT_GRASS, Biomes.PLAINS, List.of(BuiltinStructureSets.VILLAGES, BuiltinStructureSets.MINESHAFTS, BuiltinStructureSets.PILLAGER_OUTPOSTS, BuiltinStructureSets.RUINED_PORTALS, BuiltinStructureSets.STRONGHOLDS),
            true, true, new FlatLayerInfo(1, Blocks.GRASS_BLOCK), new FlatLayerInfo(3, Blocks.DIRT), new FlatLayerInfo(59, Blocks.STONE), new FlatLayerInfo(1, Blocks.BEDROCK));
        add(FlatLevelGeneratorPresets.SNOWY_KINGDOM, Blocks.SNOW, Biomes.SNOWY_PLAINS, List.of(BuiltinStructureSets.VILLAGES, BuiltinStructureSets.IGLOOS),
            false, false, new FlatLayerInfo(1, Blocks.SNOW), new FlatLayerInfo(1, Blocks.GRASS_BLOCK), new FlatLayerInfo(3, Blocks.DIRT), new FlatLayerInfo(59, Blocks.STONE), new FlatLayerInfo(1, Blocks.BEDROCK));
        add(FlatLevelGeneratorPresets.BOTTOMLESS_PIT, Items.FEATHER, Biomes.PLAINS, List.of(BuiltinStructureSets.VILLAGES),
            false, false, new FlatLayerInfo(1, Blocks.GRASS_BLOCK), new FlatLayerInfo(3, Blocks.DIRT), new FlatLayerInfo(2, Blocks.COBBLESTONE));
        add(FlatLevelGeneratorPresets.DESERT, Blocks.SAND, Biomes.DESERT, List.of(BuiltinStructureSets.VILLAGES, BuiltinStructureSets.DESERT_PYRAMIDS, BuiltinStructureSets.MINESHAFTS, BuiltinStructureSets.STRONGHOLDS),
            true, false, new FlatLayerInfo(8, Blocks.SAND), new FlatLayerInfo(52, Blocks.SANDSTONE), new FlatLayerInfo(3, Blocks.STONE), new FlatLayerInfo(1, Blocks.BEDROCK));
        add(FlatLevelGeneratorPresets.REDSTONE_READY, Items.REDSTONE, Biomes.DESERT, List.of(),
            false, false, new FlatLayerInfo(116, Blocks.SANDSTONE), new FlatLayerInfo(3, Blocks.STONE), new FlatLayerInfo(1, Blocks.BEDROCK));
        add(FlatLevelGeneratorPresets.THE_VOID, Blocks.BARRIER, Biomes.THE_VOID, List.of(),
            true, false, new FlatLayerInfo(1, Blocks.AIR));
        }
    }
}
