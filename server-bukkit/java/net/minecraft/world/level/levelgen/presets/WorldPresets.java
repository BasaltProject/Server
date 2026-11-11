// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.presets;

import java.util.List;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.Biomes;
import net.minecraft.world.level.biome.FixedBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.biome.TheEndBiomeSource;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.dimension.BuiltinDimensionTypes;
import net.minecraft.world.level.dimension.DimensionType;
import net.minecraft.world.level.dimension.LevelStem;
import net.minecraft.world.level.levelgen.DebugLevelSource;
import net.minecraft.world.level.levelgen.FlatLevelSource;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.WorldDimensions;
import net.minecraft.world.level.levelgen.flat.FlatLayerInfo;
import net.minecraft.world.level.levelgen.flat.FlatLevelGeneratorSettings;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.structure.StructureSet;

public class WorldPresets {
    public static final ResourceKey<WorldPreset> NORMAL = key("normal");
    public static final ResourceKey<WorldPreset> FLAT = key("flat");
    public static final ResourceKey<WorldPreset> FLAT_ALL_DIMENSIONS = key("flat_all_dimensions");
    public static final ResourceKey<WorldPreset> LARGE_BIOMES = key("large_biomes");
    public static final ResourceKey<WorldPreset> AMPLIFIED = key("amplified");
    public static final ResourceKey<WorldPreset> SINGLE_BIOME_SURFACE = key("single_biome_surface");
    public static final ResourceKey<WorldPreset> DEBUG = key("debug_all_block_states");

    public static void bootstrap(BootstrapContext<WorldPreset> context) {
        new Bootstrap(context).bootstrap();
    }

    private static ResourceKey<WorldPreset> key(String name) {
        return ResourceKey.create(Registries.WORLD_PRESET, Identifier.withDefaultNamespace(name));
    }

    public static Optional<ResourceKey<WorldPreset>> fromSettings(WorldDimensions dimensions) {
        return dimensions.get(LevelStem.OVERWORLD).flatMap(stem -> {
            ChunkGenerator generator = stem.generator();
            if (generator instanceof FlatLevelSource) {
                return Optional.of(FLAT);
            }
            if (generator instanceof DebugLevelSource) {
                return Optional.of(DEBUG);
            }
            return generator instanceof NoiseBasedChunkGenerator ? Optional.of(NORMAL) : Optional.empty();
        });
    }

    private static WorldDimensions dimensionsOf(HolderLookup.Provider registries, ResourceKey<WorldPreset> key) {
        return registries.lookupOrThrow(Registries.WORLD_PRESET).getOrThrow(key).value().createWorldDimensions();
    }

    public static WorldDimensions createNormalWorldDimensions(HolderLookup.Provider registries) {
        return dimensionsOf(registries, NORMAL);
    }

    public static LevelStem getNormalOverworld(HolderLookup.Provider registries) {
        return registries.lookupOrThrow(Registries.WORLD_PRESET).getOrThrow(NORMAL).value().overworld().orElseThrow();
    }

    public static WorldDimensions createTestWorldDimensions(HolderLookup.Provider registries) {
        return dimensionsOf(registries, FLAT_ALL_DIMENSIONS);
    }

    static class Bootstrap {
        private final BootstrapContext<WorldPreset> context;
        private final HolderGetter<NoiseGeneratorSettings> noiseSettings;
        private final HolderGetter<Biome> biomes;
        private final HolderGetter<PlacedFeature> placedFeatures;
        private final HolderGetter<StructureSet> structureSets;
        private final HolderGetter<MultiNoiseBiomeSourceParameterList> biomeLists;
        private final Holder<DimensionType> overworldType;
        private final Holder<DimensionType> netherType;
        private final Holder<DimensionType> endType;
        private final LevelStem netherStem;
        private final LevelStem endStem;

        private Bootstrap(BootstrapContext<WorldPreset> context) {
            this.context = context;
            HolderGetter<DimensionType> types = typed(context.lookup(Registries.DIMENSION_TYPE));
            this.noiseSettings = typed(context.lookup(Registries.NOISE_SETTINGS));
            this.biomes = context.lookup(Registries.BIOME);
            this.placedFeatures = typed(context.lookup(Registries.PLACED_FEATURE));
            this.structureSets = typed(context.lookup(Registries.STRUCTURE_SET));
            this.biomeLists = context.lookup(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST);
            this.overworldType = types.getOrThrow(BuiltinDimensionTypes.OVERWORLD);
            this.netherType = types.getOrThrow(BuiltinDimensionTypes.NETHER);
            this.endType = types.getOrThrow(BuiltinDimensionTypes.END);
            Holder<NoiseGeneratorSettings> nether = noiseSettings.getOrThrow(NoiseGeneratorSettings.NETHER);
            this.netherStem = new LevelStem(netherType, new NoiseBasedChunkGenerator(
                MultiNoiseBiomeSource.createFromPreset(biomeLists.getOrThrow(MultiNoiseBiomeSourceParameterLists.NETHER)),
                nether));
            Holder<NoiseGeneratorSettings> end = noiseSettings.getOrThrow(NoiseGeneratorSettings.END);
            this.endStem = new LevelStem(endType, new NoiseBasedChunkGenerator(TheEndBiomeSource.create(biomes), end));
        }

        @SuppressWarnings("unchecked")
        private static <T> HolderGetter<T> typed(HolderGetter<?> getter) {
            return (HolderGetter<T>) getter;
        }

        private LevelStem overworld(ChunkGenerator generator) {
            return new LevelStem(overworldType, generator);
        }

        private LevelStem noiseOverworld(BiomeSource source, ResourceKey<NoiseGeneratorSettings> settings) {
            return overworld(new NoiseBasedChunkGenerator(source, noiseSettings.getOrThrow(settings)));
        }

        private void register(ResourceKey<WorldPreset> key, LevelStem overworld) {
            context.register(key, new WorldPreset(Map.of(LevelStem.OVERWORLD, overworld, LevelStem.NETHER, netherStem,
                LevelStem.END, endStem)));
        }

        private FlatLevelGeneratorSettings flat(ResourceKey<Biome> biome, List<FlatLayerInfo> layers) {
            return FlatLevelGeneratorSettings.getDefault(biomes, structureSets, placedFeatures)
                .withBiomeAndLayers(layers, Optional.empty(), biomes.getOrThrow(biome));
        }

        private WorldPreset flatAllDimensions() {
            LevelStem top = overworld(new FlatLevelSource(flat(Biomes.DESERT,
                List.of(new FlatLayerInfo(1, Blocks.BEDROCK), new FlatLayerInfo(67, Blocks.SANDSTONE)))));
            LevelStem nether = new LevelStem(netherType, new FlatLevelSource(flat(Biomes.BASALT_DELTAS,
                List.of(new FlatLayerInfo(1, Blocks.BEDROCK), new FlatLayerInfo(3, Blocks.BASALT)))));
            LevelStem end = new LevelStem(endType, new FlatLevelSource(flat(Biomes.THE_END,
                List.of(new FlatLayerInfo(1, Blocks.BEDROCK), new FlatLayerInfo(3, Blocks.END_STONE)))));
            return new WorldPreset(Map.of(LevelStem.OVERWORLD, top, LevelStem.NETHER, nether, LevelStem.END, end));
        }

        public void bootstrap() {
            BiomeSource multiNoise = MultiNoiseBiomeSource.createFromPreset(
                biomeLists.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD));
            register(NORMAL, noiseOverworld(multiNoise, NoiseGeneratorSettings.OVERWORLD));
            register(LARGE_BIOMES, noiseOverworld(multiNoise, NoiseGeneratorSettings.LARGE_BIOMES));
            register(AMPLIFIED, noiseOverworld(multiNoise, NoiseGeneratorSettings.AMPLIFIED));
            Holder.Reference<Biome> plains = biomes.getOrThrow(Biomes.PLAINS);
            register(SINGLE_BIOME_SURFACE, noiseOverworld(new FixedBiomeSource(plains), NoiseGeneratorSettings.OVERWORLD));
            register(FLAT, overworld(new FlatLevelSource(
                FlatLevelGeneratorSettings.getDefault(biomes, structureSets, placedFeatures))));
            context.register(FLAT_ALL_DIMENSIONS, flatAllDimensions());
            register(DEBUG, overworld(new DebugLevelSource(plains)));
        }
    }
}
