// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen;

import net.minecraft.world.level.chunk.ChunkGenerator;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.biome.BiomeResolver;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
public class NoiseBasedChunkGenerator extends ChunkGenerator {
    protected NoiseBasedChunkGenerator() {
        this.settings = null;
    }
    private final Holder<NoiseGeneratorSettings> settings;
    public static final MapCodec<NoiseBasedChunkGenerator> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator.class, "CODEC");
    public NoiseBasedChunkGenerator(BiomeSource biomeSource, Holder<NoiseGeneratorSettings> settings) {
        super(biomeSource);
        this.settings = settings;
    }
    protected BiomeResolver decorateBiomeResolver(Blender blender, ChunkAccess protoChunk, BiomeResolver biomeResolver) {
        return biomeResolver;
    }
    public ChunkPos getOrigin(RandomState randomState) {
        return ChunkPos.ZERO;
    }
    protected MapCodec<? extends ChunkGenerator> codec() {
        return CODEC;
    }
    public Holder<NoiseGeneratorSettings> generatorSettings() {
        return settings;
    }
    public boolean stable(ResourceKey<NoiseGeneratorSettings> expectedPreset) {
        return settings.is(expectedPreset);
    }
    public int getBaseHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor, RandomState randomState) {
        net.minecraft.world.level.block.state.BlockState[] column = baseColumn(heightAccessor, randomState, x, z);
        int minY = settings.value().noiseSettings().clampToHeightAccessor(heightAccessor).minY();
        java.util.function.Predicate<net.minecraft.world.level.block.state.BlockState> opaque = type.isOpaque();
        for (int i = column.length - 1; i >= 0; i--) {
            if (opaque.test(column[i])) {
                return minY + i + 1;
            }
        }
        return heightAccessor.getMinY();
    }
    public NoiseColumn getBaseColumn(int x, int z, LevelHeightAccessor heightAccessor, RandomState randomState) {
        int minY = settings.value().noiseSettings().clampToHeightAccessor(heightAccessor).minY();
        return new NoiseColumn(minY, baseColumn(heightAccessor, randomState, x, z));
    }
    private net.minecraft.world.level.block.state.BlockState[] baseColumn(LevelHeightAccessor heightAccessor, RandomState randomState, int x, int z) {
        NoiseGeneratorSettings generator = settings.value();
        NoiseSettings clamped = generator.noiseSettings().clampToHeightAccessor(heightAccessor);
        if (clamped.height() <= 0) {
            return new net.minecraft.world.level.block.state.BlockState[0];
        }
        String name = org.basaltmc.bukkit.facade.Terrain.nameOf(generator);
        int[] ids = org.basaltmc.bukkit.LeftoverNatives.terrainColumn(name, randomState.seed(), x, z);
        net.minecraft.world.level.block.state.BlockState[] states = new net.minecraft.world.level.block.state.BlockState[clamped.height()];
        int offset = clamped.minY() - generator.noiseSettings().minY();
        for (int i = 0; i < states.length; i++) {
            states[i] = net.minecraft.world.level.block.Block.stateById(ids[offset + i]);
        }
        return states;
    }
    public void addDebugScreenInfo(List<String> result, RandomState randomState, BlockPos feetPos, SamplerContext samplerContext) {

    }
    public CompletableFuture<ChunkAccess> buildTerrain(ChunkAccess chunk, Blender blender, RandomState randomState, StructureManager structureManager, BiomeManager biomeManager, WorldGenRegion carverBiomeRegion, Set<Holder<Biome>> possibleBiomes) {
        return CompletableFuture.completedFuture(chunk);
    }
    public int getGenDepth() {
        return settings.value().noiseSettings().height();
    }
    public int getSeaLevel() {
        return settings.value().seaLevel();
    }
    public int getMinY() {
        return settings.value().noiseSettings().minY();
    }
    public void spawnOriginalMobs(WorldGenRegion worldGenRegion) {

    }
}
