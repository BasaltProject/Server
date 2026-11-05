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
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator#getBaseHeight");
    }
    public NoiseColumn getBaseColumn(int x, int z, LevelHeightAccessor heightAccessor, RandomState randomState) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator#getBaseColumn");
    }
    public void addDebugScreenInfo(List<String> result, RandomState randomState, BlockPos feetPos, SamplerContext samplerContext) {

    }
    public CompletableFuture<ChunkAccess> buildTerrain(ChunkAccess chunk, Blender blender, RandomState randomState, StructureManager structureManager, BiomeManager biomeManager, WorldGenRegion carverBiomeRegion, Set<Holder<Biome>> possibleBiomes) {
        return CompletableFuture.completedFuture(chunk);
    }
    public int getGenDepth() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator#getGenDepth");
    }
    public int getSeaLevel() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator#getSeaLevel");
    }
    public int getMinY() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator#getMinY");
    }
    public void spawnOriginalMobs(WorldGenRegion worldGenRegion) {

    }
}
