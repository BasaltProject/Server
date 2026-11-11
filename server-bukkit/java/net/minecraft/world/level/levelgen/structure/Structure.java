// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.structure;

import java.util.function.Predicate;
import net.minecraft.core.Holder;
import net.minecraft.core.RegistryAccess;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import java.util.Map;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.core.QuartPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.MobCategory;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.GenerationStep;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.structure.pieces.PiecesContainer;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
public abstract class Structure {
    protected Structure() {
    }
    public StructureStart generate(Holder<Structure> selected, ResourceKey<Level> dimension, RegistryAccess registryAccess, ChunkGenerator chunkGenerator, BiomeSource biomeSource, Climate.Sampler climateSampler, RandomState randomState, StructureTemplateManager structureTemplateManager, long seed, ChunkPos sourceChunkPos, int references, LevelHeightAccessor heightAccessor, Predicate<Holder<Biome>> validBiome) {
        GenerationContext ctx = new GenerationContext(registryAccess, chunkGenerator, biomeSource, climateSampler, randomState, structureTemplateManager, seed, sourceChunkPos, heightAccessor, validBiome);
        Optional<GenerationStub> generation = findValidGenerationPoint(ctx);
        if (generation.isPresent()) {
            StructureStart start = new StructureStart(this, sourceChunkPos, references, generation.get().getPiecesBuilder().build());
            if (start.isValid()) {
                return start;
            }
        }
        StructureStart none = StructureStart.INVALID_START;
        return none;
    }
    public static final Codec<Structure> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.Structure.class, "DIRECT_CODEC");
    public static final Codec<Holder<Structure>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.Structure.class, "CODEC");
    protected StructureSettings settings;
    public static <S extends Structure> RecordCodecBuilder<S, StructureSettings> settingsCodec(RecordCodecBuilder.Instance<S> i) {
        return StructureSettings.CODEC.forGetter(structure -> structure.settings);
    }
    public static <S extends Structure> MapCodec<S> simpleCodec(Function<StructureSettings, S> constructor) {
        return RecordCodecBuilder.mapCodec(i -> i.group(settingsCodec(i)).apply(i, constructor));
    }
    protected Structure(StructureSettings settings) {
        this.settings = settings;
    }
    public HolderSet<Biome> biomes() {
        return settings.biomes();
    }
    public Map<MobCategory, StructureSpawnOverride> spawnOverrides() {
        return settings.spawnOverrides();
    }
    public GenerationStep.Decoration step() {
        return settings.step();
    }
    public TerrainAdjustment terrainAdaptation() {
        return settings.terrainAdaptation();
    }
    public BoundingBox adjustBoundingBox(BoundingBox boundingBox) {
        return terrainAdaptation() != TerrainAdjustment.NONE ? boundingBox.inflatedBy(12) : boundingBox;
    }
    protected static Optional<GenerationStub> onTopOfChunkCenter(GenerationContext context, Heightmap.Types heightmap, Consumer<StructurePiecesBuilder> generator) {
        return context.couldValidBiomeExistOnTopOfChunkCenter() ? onTopOfChunkCenterWithoutBiomeCheck(context, heightmap, generator) : Optional.empty();
    }
    protected static Optional<GenerationStub> onTopOfChunkCenterWithoutBiomeCheck(GenerationContext context, Heightmap.Types heightmap, Consumer<StructurePiecesBuilder> generator) {
        ChunkPos chunk = context.chunkPos();
        int x = chunk.getMiddleBlockX();
        int z = chunk.getMiddleBlockZ();
        int y = context.chunkGenerator().getFirstOccupiedHeight(x, z, heightmap, context.heightAccessor(), context.randomState());
        return Optional.of(new GenerationStub(new BlockPos(x, y, z), generator));
    }
    public void afterPlace(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, PiecesContainer pieces) {
    }

    private static int[] cornerHeights(GenerationContext context, int minX, int sizeX, int minZ, int sizeZ) {
        ChunkGenerator generator = context.chunkGenerator();
        int[] heights = new int[4];
        for (int corner = 0; corner < 4; corner++) {
            heights[corner] = generator.getFirstOccupiedHeight(minX + (corner >> 1) * sizeX, minZ + (corner & 1) * sizeZ,
                Heightmap.Types.WORLD_SURFACE_WG, context.heightAccessor(), context.randomState());
        }
        return heights;
    }
    public static int getMeanFirstOccupiedHeight(GenerationContext context, int minX, int sizeX, int minZ, int sizeZ) {
        int[] heights = cornerHeights(context, minX, sizeX, minZ, sizeZ);
        return (heights[0] + heights[1] + heights[2] + heights[3]) / 4;
    }
    protected static int getLowestY(GenerationContext context, int sizeX, int sizeZ) {
        return getLowestY(context, context.chunkPos().getMinBlockX(), context.chunkPos().getMinBlockZ(), sizeX, sizeZ);
    }
    protected static int getLowestY(GenerationContext context, int minX, int minZ, int sizeX, int sizeZ) {
        int[] heights = cornerHeights(context, minX, sizeX, minZ, sizeZ);
        return Math.min(Math.min(heights[0], heights[1]), Math.min(heights[2], heights[3]));
    }
    protected BlockPos getLowestYIn5by5Box(GenerationContext context, int blockX, int blockZ, Rotation rotation) {
        int offsetX = rotation == Rotation.CLOCKWISE_90 || rotation == Rotation.CLOCKWISE_180 ? -5 : 5;
        int offsetZ = rotation == Rotation.CLOCKWISE_180 || rotation == Rotation.COUNTERCLOCKWISE_90 ? -5 : 5;
        return new BlockPos(blockX, getLowestY(context, blockX, blockZ, offsetX, offsetZ), blockZ);
    }
    protected abstract Optional<GenerationStub> findGenerationPoint(GenerationContext context);
    public Optional<GenerationStub> findValidGenerationPoint(GenerationContext context) {
        return findGenerationPoint(context).filter(context::isValidBiome);
    }
    public abstract StructureType<?> type();
    public static record StructureSettings(HolderSet<Biome> biomes, Map<MobCategory, StructureSpawnOverride> spawnOverrides, GenerationStep.Decoration step, TerrainAdjustment terrainAdaptation) {
        public static final MapCodec<StructureSettings> CODEC = RecordCodecBuilder.mapCodec(i -> i.group(
            org.basaltmc.bukkit.facade.ReflectiveCodecs.holderSet(net.minecraft.core.registries.Registries.BIOME).fieldOf("biomes").forGetter(StructureSettings::biomes),
            Codec.simpleMap(MobCategory.CODEC, StructureSpawnOverride.CODEC, net.minecraft.util.StringRepresentable.keys(MobCategory.values())).fieldOf("spawn_overrides")
                .forGetter(StructureSettings::spawnOverrides),
            GenerationStep.Decoration.CODEC.fieldOf("step").forGetter(StructureSettings::step),
            TerrainAdjustment.CODEC.optionalFieldOf("terrain_adaptation", TerrainAdjustment.NONE).forGetter(StructureSettings::terrainAdaptation)).apply(i, StructureSettings::new));
        public StructureSettings(HolderSet<Biome> biomes) {
            this(biomes, Map.of(), GenerationStep.Decoration.SURFACE_STRUCTURES, TerrainAdjustment.NONE);
        }
        public static class Builder {
            private final HolderSet<Biome> biomes;
            private Map<MobCategory, StructureSpawnOverride> spawnOverrides = Map.of();
            private GenerationStep.Decoration step = GenerationStep.Decoration.SURFACE_STRUCTURES;
            private TerrainAdjustment terrainAdaption = TerrainAdjustment.NONE;
            public Builder(HolderSet<Biome> biomes) {
                this.biomes = biomes;
            }
            public Builder spawnOverrides(Map<MobCategory, StructureSpawnOverride> spawnOverrides) {
                this.spawnOverrides = spawnOverrides;
                return this;
            }
            public Builder generationStep(GenerationStep.Decoration step) {
                this.step = step;
                return this;
            }
            public Builder terrainAdapation(TerrainAdjustment terrainAdaption) {
                this.terrainAdaption = terrainAdaption;
                return this;
            }
            public StructureSettings build() {
                return new StructureSettings(biomes, spawnOverrides, step, terrainAdaption);
            }
        }
    }
    public static record GenerationContext(RegistryAccess registryAccess, ChunkGenerator chunkGenerator, BiomeSource biomeSource, Climate.Sampler climateSampler, BiomeResolver biomeResolver, RandomState randomState, StructureTemplateManager structureTemplateManager, WorldgenRandom random, long seed, ChunkPos chunkPos, LevelHeightAccessor heightAccessor, Predicate<Holder<Biome>> validBiome) {
        public GenerationContext(RegistryAccess registryAccess, ChunkGenerator chunkGenerator, BiomeSource biomeSource, Climate.Sampler climateSampler, RandomState randomState, StructureTemplateManager structureTemplateManager, long seed, ChunkPos chunkPos, LevelHeightAccessor heightAccessor, Predicate<Holder<Biome>> validBiome) {
            this(registryAccess, chunkGenerator, biomeSource, climateSampler, biomeSource.createResolver(climateSampler), randomState, structureTemplateManager, randomFor(seed, chunkPos), seed, chunkPos, heightAccessor, validBiome);
        }
        private static WorldgenRandom randomFor(long seed, ChunkPos chunkPos) {
            WorldgenRandom random = new WorldgenRandom(new net.minecraft.world.level.levelgen.LegacyRandomSource(0L));
            random.setLargeFeatureSeed(seed, chunkPos.x(), chunkPos.z());
            return random;
        }
        public boolean isValidBiome(GenerationStub stub) {
            BlockPos at = stub.position();
            return validBiome.test(biomeResolver.getNoiseBiome(QuartPos.fromBlock(at.getX()), QuartPos.fromBlock(at.getY()), QuartPos.fromBlock(at.getZ())));
        }
        public boolean couldStructureExistInColumn(int blockX, int blockZ, int minBlockY, int maxBlockY) {
            int quartZ = QuartPos.fromBlock(blockZ);
            int quartX = QuartPos.fromBlock(blockX);
            int minQuart = QuartPos.fromBlock(minBlockY);
            int maxQuart = QuartPos.fromBlock(maxBlockY);
            BiomeResolver column = biomeSource.createResolverForChunk(climateSampler, quartX, minQuart, quartZ, 1, maxQuart - minQuart + 1, 1);
            for (int quartY = minQuart; quartY <= maxQuart; quartY++) {
                if (validBiome.test(column.getNoiseBiome(quartX, quartY, quartZ))) {
                    return true;
                }
            }
            return false;
        }
        public boolean couldValidBiomeExistInTerrainColumn(int blockX, int blockZ) {
            return couldStructureExistInColumn(blockX, blockZ, heightAccessor.getMinY() - 1, heightAccessor.getMaxY());
        }
        public boolean couldValidBiomeExistOnTopOfChunkCenter() {
            return couldValidBiomeExistInTerrainColumn(chunkPos.getMiddleBlockX(), chunkPos.getMiddleBlockZ());
        }
    }
    public static record GenerationStub(BlockPos position, Either<Consumer<StructurePiecesBuilder>, StructurePiecesBuilder> generator) {
        public GenerationStub(BlockPos position, Consumer<StructurePiecesBuilder> generator) {
            this(position, Either.left(generator));
        }
        public StructurePiecesBuilder getPiecesBuilder() {
            return generator.map(consumer -> {
                StructurePiecesBuilder fresh = new StructurePiecesBuilder();
                consumer.accept(fresh);
                return fresh;
            }, existing -> existing);
        }
    }
}
