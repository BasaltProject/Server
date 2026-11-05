// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.chunk;

import net.minecraft.world.level.biome.BiomeSource;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.function.Function;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderSet;
import net.minecraft.core.RegistryAccess;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.entity.MobCategory;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeGenerationSettings;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.biome.MobSpawnSettings;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureSet;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;
public abstract class ChunkGenerator {
    protected ChunkGenerator() {
        this.biomeSource = null;
        this.generationSettingsGetter = biome -> biome.value().getGenerationSettings();
    }
    public BiomeSource getBiomeSource() {
        return biomeSource;
    }
    public static final Codec<ChunkGenerator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.chunk.ChunkGenerator.class, "CODEC");
    protected final BiomeSource biomeSource;
    private final Function<Holder<Biome>, BiomeGenerationSettings> generationSettingsGetter;
    public ChunkGenerator(BiomeSource biomeSource) {
        this(biomeSource, biome -> biome.value().getGenerationSettings());
    }
    public ChunkGenerator(BiomeSource biomeSource, Function<Holder<Biome>, BiomeGenerationSettings> generationSettingsGetter) {
        this.biomeSource = biomeSource;
        this.generationSettingsGetter = generationSettingsGetter;
    }
    public void validate() {

    }
    public ChunkPos getOrigin(RandomState randomState) {
        return ChunkPos.ZERO;
    }
    protected abstract MapCodec<? extends ChunkGenerator> codec();
    public ChunkGeneratorStructureState createState(HolderLookup<StructureSet> structureSets, RandomState randomState, long legacyLevelSeed) {
        return ChunkGeneratorStructureState.ofSeed(randomState, legacyLevelSeed);
    }
    public Optional<Identifier> getTypeNameForDataFixer() {
        return Optional.empty();
    }
    public CompletableFuture<ChunkAccess> createBiomes(RandomState randomState, Blender blender, StructureManager structureManager, ChunkAccess protoChunk) {
        return CompletableFuture.completedFuture(protoChunk);
    }
    protected BiomeResolver decorateBiomeResolver(Blender blender, ChunkAccess protoChunk, BiomeResolver biomeResolver) {
        return biomeResolver;
    }
    public Pair<BlockPos, Holder<Structure>> findNearestMapStructure(ServerLevel level, HolderSet<Structure> wantedStructures, BlockPos pos, int maxSearchRadius, boolean createReference) {
        BlockPos best = null;
        Holder<Structure> bestHolder = null;
        double bestDistance = Double.MAX_VALUE;
        for (Holder<Structure> holder : wantedStructures) {
            BlockPos found = level.findNearestMapStructure(HolderSet.direct(holder), pos, maxSearchRadius, createReference);
            if (found != null && found.distSqr(pos) < bestDistance) {
                best = found;
                bestHolder = holder;
                bestDistance = found.distSqr(pos);
            }
        }
        return best == null ? null : Pair.of(best, bestHolder);
    }
    public void applyBiomeDecoration(WorldGenLevel level, ChunkAccess chunk, StructureManager structureManager) {

    }
    public void spawnOriginalMobs(WorldGenRegion worldGenRegion) {

    }
    public int getSpawnHeight(LevelHeightAccessor heightAccessor) {
        return 64;
    }
    public abstract int getGenDepth();
    public WeightedList<MobSpawnSettings.SpawnerData> getMobsAt(Level level, StructureManager structureManager, MobCategory mobCategory, BlockPos pos) {
        return WeightedList.of();
    }
    public void createStructures(RegistryAccess registryAccess, ChunkGeneratorStructureState state, StructureManager structureManager, ChunkAccess centerChunk, StructureTemplateManager structureTemplateManager, ResourceKey<Level> level) {

    }
    public void createReferences(WorldGenLevel level, StructureManager structureManager, ChunkAccess centerChunk) {

    }
    public CompletableFuture<ChunkAccess> buildTerrain(ChunkAccess chunk, Blender blender, RandomState randomState, StructureManager structureManager, BiomeManager biomeManager, WorldGenRegion carverBiomeRegion, Set<Holder<Biome>> possibleBiomes) {
        return CompletableFuture.completedFuture(chunk);
    }
    public abstract int getSeaLevel();
    public abstract int getMinY();
    public abstract int getBaseHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor, RandomState randomState);
    public abstract NoiseColumn getBaseColumn(int x, int z, LevelHeightAccessor heightAccessor, RandomState randomState);
    public int getFirstFreeHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor, RandomState randomState) {
        return getBaseHeight(x, z, type, heightAccessor, randomState);
    }
    public int getFirstOccupiedHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor, RandomState randomState) {
        return getBaseHeight(x, z, type, heightAccessor, randomState) - 1;
    }
    public void addDebugScreenInfo(List<String> result, RandomState randomState, BlockPos feetPos, SamplerContext samplerContext) {

    }
    public BiomeGenerationSettings getBiomeGenerationSettings(Holder<Biome> biome) {
        return generationSettingsGetter.apply(biome);
    }
}
