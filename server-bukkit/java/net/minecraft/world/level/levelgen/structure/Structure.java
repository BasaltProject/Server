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
    public StructureStart generate(Holder selected, ResourceKey dimension, RegistryAccess registryAccess, ChunkGenerator chunkGenerator, BiomeSource biomeSource, Climate.Sampler climateSampler, RandomState randomState, StructureTemplateManager structureTemplateManager, long seed, ChunkPos sourceChunkPos, int references, LevelHeightAccessor heightAccessor, Predicate validBiome) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.structure.Structure.generate");
    }
    public static final Codec<Structure> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.Structure.class, "DIRECT_CODEC");
    public static final Codec<Holder<Structure>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.structure.Structure.class, "CODEC");
    protected StructureSettings settings;
    public static <S extends Structure> RecordCodecBuilder<S, StructureSettings> settingsCodec(RecordCodecBuilder.Instance<S> i) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#settingsCodec");
    }
    public static <S extends Structure> MapCodec<S> simpleCodec(Function<StructureSettings, S> constructor) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#simpleCodec");
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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#adjustBoundingBox");
    }
    protected static Optional<GenerationStub> onTopOfChunkCenter(GenerationContext context, Heightmap.Types heightmap, Consumer<StructurePiecesBuilder> generator) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#onTopOfChunkCenter");
    }
    protected static Optional<GenerationStub> onTopOfChunkCenterWithoutBiomeCheck(GenerationContext context, Heightmap.Types heightmap, Consumer<StructurePiecesBuilder> generator) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#onTopOfChunkCenterWithoutBiomeCheck");
    }
    public void afterPlace(WorldGenLevel level, StructureManager structureManager, ChunkGenerator generator, RandomSource random, BoundingBox chunkBB, ChunkPos chunkPos, PiecesContainer pieces) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#afterPlace");
    }
    public static int getMeanFirstOccupiedHeight(GenerationContext context, int minX, int sizeX, int minZ, int sizeZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#getMeanFirstOccupiedHeight");
    }
    protected static int getLowestY(GenerationContext context, int sizeX, int sizeZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#getLowestY");
    }
    protected static int getLowestY(GenerationContext context, int minX, int minZ, int sizeX, int sizeZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#getLowestY");
    }
    protected BlockPos getLowestYIn5by5Box(GenerationContext context, int blockX, int blockZ, Rotation rotation) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#getLowestYIn5by5Box");
    }
    protected Optional<GenerationStub> findGenerationPoint(GenerationContext context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#findGenerationPoint");
    }
    public Optional<GenerationStub> findValidGenerationPoint(GenerationContext context) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#findValidGenerationPoint");
    }
    public StructureType<?> type() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure#type");
    }
    public static record StructureSettings(HolderSet<Biome> biomes, Map<MobCategory, StructureSpawnOverride> spawnOverrides, GenerationStep.Decoration step, TerrainAdjustment terrainAdaptation) {
        public static final MapCodec<StructureSettings> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.structure.Structure.StructureSettings.class, "CODEC");
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
            this((RegistryAccess) null, (ChunkGenerator) null, (BiomeSource) null, (Climate.Sampler) null, (BiomeResolver) null, (RandomState) null, (StructureTemplateManager) null, (WorldgenRandom) null, (long) 0, (ChunkPos) null, (LevelHeightAccessor) null, (Predicate) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure.GenerationContext#<init>");
        }
        public boolean isValidBiome(GenerationStub stub) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure.GenerationContext#isValidBiome");
        }
        public boolean couldStructureExistInColumn(int blockX, int blockZ, int minBlockY, int maxBlockY) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure.GenerationContext#couldStructureExistInColumn");
        }
        public boolean couldValidBiomeExistInTerrainColumn(int blockX, int blockZ) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure.GenerationContext#couldValidBiomeExistInTerrainColumn");
        }
        public boolean couldValidBiomeExistOnTopOfChunkCenter() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure.GenerationContext#couldValidBiomeExistOnTopOfChunkCenter");
        }
    }
    public static record GenerationStub(BlockPos position, Either<Consumer<StructurePiecesBuilder>, StructurePiecesBuilder> generator) {
        public GenerationStub(BlockPos position, Consumer<StructurePiecesBuilder> generator) {
            this((BlockPos) null, (Either) null); throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure.GenerationStub#<init>");
        }
        public StructurePiecesBuilder getPiecesBuilder() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.structure.Structure.GenerationStub#getPiecesBuilder");
        }
    }
}
