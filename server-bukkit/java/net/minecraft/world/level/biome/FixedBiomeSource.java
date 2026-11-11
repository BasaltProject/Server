// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;
import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.MapCodec;
import java.util.Set;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.levelgen.RandomState;
import com.google.common.collect.Sets;
import net.minecraft.util.Mth;
public class FixedBiomeSource extends BiomeSource implements BiomeResolver {
    public static final MapCodec<FixedBiomeSource> CODEC = Biome.CODEC.fieldOf("biome").xmap(FixedBiomeSource::new, source -> source.biome).stable();
    private final Holder<Biome> biome;
    public FixedBiomeSource(Holder<Biome> biome) {
        this.biome = biome;
    }
    @Override
    protected Stream<Holder<Biome>> collectPossibleBiomes() {
        return Stream.of(biome);
    }
    @Override
    protected MapCodec<FixedBiomeSource> codec() {
        return CODEC;
    }
    @Override
    public BiomeResolver createResolver(Climate.Sampler sampler) {
        return this;
    }
    @Override
    public Holder<Biome> getNoiseBiome(int quartX, int quartY, int quartZ) {
        return biome;
    }
    @Override
    public Pair<BlockPos, Holder<Biome>> findBiomeHorizontal(int originX, int originY, int originZ, int r, int skipStep, Predicate<Holder<Biome>> allowed, RandomSource random, boolean findClosest, RandomState randomState) {
        if (!allowed.test(biome)) {
            return null;
        }
        if (findClosest) {
            return Pair.of(new BlockPos(originX, originY, originZ), biome);
        }
        int dx = random.nextInt(r * 2 + 1);
        int dz = random.nextInt(r * 2 + 1);
        return Pair.of(new BlockPos(originX - r + dx, originY, originZ - r + dz), biome);
    }
    @Override
    public Pair<BlockPos, Holder<Biome>> findClosestBiome3d(BlockPos origin, int searchRadius, int sampleResolutionHorizontal, int sampleResolutionVertical, Predicate<Holder<Biome>> allowed, RandomState randomState, LevelReader level) {
        return allowed.test(biome) ? Pair.of(origin.atY(Mth.clamp(origin.getY(), level.getMinY() + 1, level.getMaxY() + 1)), biome) : null;
    }
    @Override
    public Set<Holder<Biome>> getBiomesWithin(int x, int y, int z, int radius) {
        return Sets.newHashSet(Set.of(biome));
    }
}
