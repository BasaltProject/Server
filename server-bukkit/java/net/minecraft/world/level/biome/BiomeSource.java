// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.levelgen.RandomState;
public abstract class BiomeSource {
    protected BiomeSource() {
    }
    public static final Codec<BiomeSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.BiomeSource.class, "CODEC");
    protected MapCodec<? extends BiomeSource> codec() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#codec");
    }
    protected Stream<Holder<Biome>> collectPossibleBiomes() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#collectPossibleBiomes");
    }
    public Set<Holder<Biome>> possibleBiomes() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#possibleBiomes");
    }
    public Pair<BlockPos, Holder<Biome>> findBiomeHorizontal(int x, int y, int z, int searchRadius, Predicate<Holder<Biome>> allowed, RandomSource random, RandomState randomState) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#findBiomeHorizontal");
    }
    public Pair<BlockPos, Holder<Biome>> findClosestBiome3d(BlockPos origin, int searchRadius, int sampleResolutionHorizontal, int sampleResolutionVertical, Predicate<Holder<Biome>> allowed, RandomState randomState, LevelReader level) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#findClosestBiome3d");
    }
    public Pair<BlockPos, Holder<Biome>> findBiomeHorizontal(int originX, int originY, int originZ, int searchRadius, int skipSteps, Predicate<Holder<Biome>> allowed, RandomSource random, boolean findClosest, RandomState randomState) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#findBiomeHorizontal");
    }
    public BiomeResolver createUncachedResolver(RandomState randomState) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#createUncachedResolver");
    }
    public BiomeResolver createCachingResolver(RandomState randomState) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#createCachingResolver");
    }
    public BiomeResolver createResolver(Climate.Sampler sampler) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#createResolver");
    }
    public BiomeResolver createResolverForChunk(Climate.Sampler sampler, int minQuartX, int minQuartY, int minQuartZ, int quartSizeX, int quartSizeY, int quartSizeZ) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#createResolverForChunk");
    }
    public void addDebugInfo(List<String> result, BlockPos feetPos, Climate.Sampler sampler) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.biome.BiomeSource#addDebugInfo");
    }
}
