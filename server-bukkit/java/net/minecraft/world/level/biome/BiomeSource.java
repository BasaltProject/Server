// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.Set;
import java.util.function.Predicate;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.QuartPos;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;

public abstract class BiomeSource {
    public static final Codec<BiomeSource> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.biome.BiomeSource.class, "CODEC");
    private volatile Set<Holder<Biome>> possibleBiomes;

    protected BiomeSource() {
    }

    protected abstract MapCodec<? extends BiomeSource> codec();

    protected abstract Stream<Holder<Biome>> collectPossibleBiomes();

    public Set<Holder<Biome>> possibleBiomes() {
        Set<Holder<Biome>> known = possibleBiomes;
        if (known == null) {
            known = collectPossibleBiomes().distinct().collect(Collectors.toUnmodifiableSet());
            possibleBiomes = known;
        }
        return known;
    }

    public Pair<BlockPos, Holder<Biome>> findBiomeHorizontal(int x, int y, int z, int searchRadius, Predicate<Holder<Biome>> allowed,
        RandomSource random, RandomState randomState) {
        return findBiomeHorizontal(x, y, z, searchRadius, 1, allowed, random, false, randomState);
    }

    public Pair<BlockPos, Holder<Biome>> findClosestBiome3d(BlockPos origin, int searchRadius, int sampleResolutionHorizontal,
        int sampleResolutionVertical, Predicate<Holder<Biome>> allowed, RandomState randomState, LevelReader level) {
        Set<Holder<Biome>> candidates = possibleBiomes().stream().filter(allowed).collect(Collectors.toUnmodifiableSet());
        if (candidates.isEmpty()) {
            return null;
        }
        BiomeResolver resolver = createCachingResolver(randomState);
        int rings = Math.floorDiv(searchRadius, sampleResolutionHorizontal);
        int[] heights = Mth.outFromOrigin(origin.getY(), level.getMinY() + 1, level.getMaxY() + 1, sampleResolutionVertical).toArray();
        for (BlockPos.MutableBlockPos column : BlockPos.spiralAround(BlockPos.ZERO, rings, Direction.EAST, Direction.SOUTH)) {
            int blockX = origin.getX() + column.getX() * sampleResolutionHorizontal;
            int blockZ = origin.getZ() + column.getZ() * sampleResolutionHorizontal;
            for (int blockY : heights) {
                Holder<Biome> biome = resolver.getNoiseBiome(QuartPos.fromBlock(blockX), QuartPos.fromBlock(blockY), QuartPos.fromBlock(blockZ));
                if (candidates.contains(biome)) {
                    return Pair.of(new BlockPos(blockX, blockY, blockZ), biome);
                }
            }
        }
        return null;
    }

    public Pair<BlockPos, Holder<Biome>> findBiomeHorizontal(int originX, int originY, int originZ, int searchRadius, int skipSteps,
        Predicate<Holder<Biome>> allowed, RandomSource random, boolean findClosest, RandomState randomState) {
        int centerX = QuartPos.fromBlock(originX);
        int centerZ = QuartPos.fromBlock(originZ);
        int radius = QuartPos.fromBlock(searchRadius);
        int quartY = QuartPos.fromBlock(originY);
        BiomeResolver resolver = createCachingResolver(randomState);
        Pair<BlockPos, Holder<Biome>> result = null;
        int matches = 0;
        for (int ring = findClosest ? 0 : radius; ring <= radius; ring += skipSteps) {
            for (int dz = -ring; dz <= ring; dz += skipSteps) {
                boolean zEdge = Math.abs(dz) == ring;
                for (int dx = -ring; dx <= ring; dx += skipSteps) {
                    if (findClosest && !zEdge && Math.abs(dx) != ring) {
                        continue;
                    }
                    int quartX = centerX + dx;
                    int quartZ = centerZ + dz;
                    Holder<Biome> biome = resolver.getNoiseBiome(quartX, quartY, quartZ);
                    if (allowed.test(biome)) {
                        if (result == null || random.nextInt(matches + 1) == 0) {
                            BlockPos at = new BlockPos(QuartPos.toBlock(quartX), originY, QuartPos.toBlock(quartZ));
                            if (findClosest) {
                                return Pair.of(at, biome);
                            }
                            result = Pair.of(at, biome);
                        }
                        matches++;
                    }
                }
            }
        }
        return result;
    }

    public final BiomeResolver createUncachedResolver(RandomState randomState) {
        return createResolver(randomState.createClimateSampler(SamplerContext.EMPTY_UNCACHED));
    }

    public final BiomeResolver createCachingResolver(RandomState randomState) {
        return createResolver(randomState.createClimateSampler(SamplerContext.builder().enableCaches().build()));
    }

    public abstract BiomeResolver createResolver(Climate.Sampler sampler);

    public BiomeResolver createResolverForChunk(Climate.Sampler sampler, int minQuartX, int minQuartY, int minQuartZ, int quartSizeX,
        int quartSizeY, int quartSizeZ) {
        return createResolver(sampler);
    }

    public void addDebugInfo(List<String> result, BlockPos feetPos, Climate.Sampler sampler) {
    }
}
