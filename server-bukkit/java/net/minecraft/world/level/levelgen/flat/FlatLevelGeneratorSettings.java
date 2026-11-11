// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.flat;
import java.util.ArrayList;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.levelgen.GenerationStep;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.feature.FillLayerFeature;
import net.minecraft.data.worldgen.placement.PlacementUtils;
import net.minecraft.data.worldgen.placement.MiscOverworldPlacements;
import net.minecraft.world.level.biome.Biomes;
import net.minecraft.world.level.levelgen.structure.BuiltinStructureSets;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeGenerationSettings;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.structure.StructureSet;
public class FlatLevelGeneratorSettings {
    private final Optional<HolderSet<StructureSet>> structureOverrides;
    private final List<FlatLayerInfo> layersInfo = new ArrayList<>();
    private final Holder<Biome> biome;
    private final List<BlockState> layers = new ArrayList<>();
    private boolean voidGen;
    private boolean decoration;
    private boolean addLakes;
    private final List<Holder<PlacedFeature>> lakes;
    public static final Codec<FlatLevelGeneratorSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.flat.FlatLevelGeneratorSettings.class, "CODEC");
    private FlatLevelGeneratorSettings(Optional<HolderSet<StructureSet>> structureOverrides, List<FlatLayerInfo> layers, boolean lakes, boolean features, Optional<Holder<Biome>> biome, Holder.Reference<Biome> fallbackBiome, Holder<PlacedFeature> lavaUnderground, Holder<PlacedFeature> lavaSurface) {
        this(structureOverrides, biome.orElse(fallbackBiome), List.of(lavaUnderground, lavaSurface));
        if (lakes) {
            setAddLakes();
        }
        if (features) {
            setDecoration();
        }
        layersInfo.addAll(layers);
        updateLayers();
    }
    public FlatLevelGeneratorSettings(Optional<HolderSet<StructureSet>> structureOverrides, Holder<Biome> biome, List<Holder<PlacedFeature>> lakes) {
        this.structureOverrides = structureOverrides;
        this.biome = biome;
        this.lakes = lakes;
    }
    public FlatLevelGeneratorSettings withBiomeAndLayers(List<FlatLayerInfo> layers, Optional<HolderSet<StructureSet>> structureOverrides, Holder<Biome> biome) {
        FlatLevelGeneratorSettings copy = new FlatLevelGeneratorSettings(structureOverrides, biome, lakes);
        for (FlatLayerInfo layer : layers) {
            copy.layersInfo.add(layer);
            copy.updateLayers();
        }
        if (decoration) {
            copy.setDecoration();
        }
        if (addLakes) {
            copy.setAddLakes();
        }
        return copy;
    }
    public void setDecoration() {
        decoration = true;
    }
    public void setAddLakes() {
        addLakes = true;
    }
    public BiomeGenerationSettings adjustGenerationSettings(Holder<Biome> sourceBiome) {
        if (!sourceBiome.equals(biome)) {
            return sourceBiome.value().getGenerationSettings();
        }
        BiomeGenerationSettings own = biome.value().getGenerationSettings();
        BiomeGenerationSettings.PlainBuilder built = new BiomeGenerationSettings.PlainBuilder();
        if (addLakes) {
            for (Holder<PlacedFeature> lake : lakes) {
                built.addFeature(GenerationStep.Decoration.LAKES, lake);
            }
        }
        if ((!voidGen || sourceBiome.is(Biomes.THE_VOID)) && decoration) {
            List<HolderSet<PlacedFeature>> steps = own.features();
            for (int step = 0; step < steps.size(); step++) {
                boolean structureStep = step == GenerationStep.Decoration.UNDERGROUND_STRUCTURES.ordinal()
                    || step == GenerationStep.Decoration.SURFACE_STRUCTURES.ordinal();
                if (structureStep || addLakes && step == GenerationStep.Decoration.LAKES.ordinal()) {
                    continue;
                }
                for (Holder<PlacedFeature> feature : steps.get(step)) {
                    built.addFeature(step, feature);
                }
            }
        }
        for (int i = 0; i < layers.size(); i++) {
            BlockState layer = layers.get(i);
            if (!Heightmap.Types.MOTION_BLOCKING.isOpaque().test(layer)) {
                layers.set(i, null);
                built.addFeature(GenerationStep.Decoration.TOP_LAYER_MODIFICATION,
                    PlacementUtils.inlinePlaced(new FillLayerFeature(i, layer)));
            }
        }
        return built.build();
    }
    public Optional<HolderSet<StructureSet>> structureOverrides() {
        return structureOverrides;
    }
    public Holder<Biome> getBiome() {
        return biome;
    }
    public List<FlatLayerInfo> getLayersInfo() {
        return layersInfo;
    }
    public List<BlockState> getLayers() {
        return layers;
    }
    public void updateLayers() {
        layers.clear();
        boolean empty = true;
        for (FlatLayerInfo info : layersInfo) {
            for (int y = 0; y < info.getHeight(); y++) {
                layers.add(info.getBlockState());
                empty &= info.getBlockState().is(Blocks.AIR);
            }
        }
        voidGen = empty;
    }
    public static FlatLevelGeneratorSettings getDefault(HolderGetter<Biome> biomes, HolderGetter<StructureSet> structureSets, HolderGetter<PlacedFeature> placedFeatures) {
        HolderSet<StructureSet> sets = HolderSet.direct(structureSets.getOrThrow(BuiltinStructureSets.STRONGHOLDS),
            structureSets.getOrThrow(BuiltinStructureSets.VILLAGES));
        FlatLevelGeneratorSettings result = new FlatLevelGeneratorSettings(Optional.of(sets), getDefaultBiome(biomes),
            createLakesList(placedFeatures));
        result.getLayersInfo().add(new FlatLayerInfo(1, Blocks.BEDROCK));
        result.getLayersInfo().add(new FlatLayerInfo(2, Blocks.DIRT));
        result.getLayersInfo().add(new FlatLayerInfo(1, Blocks.GRASS_BLOCK));
        result.updateLayers();
        return result;
    }
    public static Holder<Biome> getDefaultBiome(HolderGetter<Biome> biomes) {
        return biomes.getOrThrow(Biomes.PLAINS);
    }
    public static List<Holder<PlacedFeature>> createLakesList(HolderGetter<PlacedFeature> placedFeatures) {
        return List.of(placedFeatures.getOrThrow(MiscOverworldPlacements.LAKE_LAVA_UNDERGROUND),
            placedFeatures.getOrThrow(MiscOverworldPlacements.LAKE_LAVA_SURFACE));

    }
}
