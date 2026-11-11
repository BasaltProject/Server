// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record RandomSelectorFeature(List<WeightedPlacedFeature> features, Holder<PlacedFeature> defaultFeature) implements Feature {
    public static final MapCodec<RandomSelectorFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.RandomSelectorFeature.class, "CODEC");
    public MapCodec<RandomSelectorFeature> codec() {
        return CODEC;
    }
    public Stream<Holder<Feature>> getSubFeatures() {
        return Stream.concat(features.stream().flatMap(w -> w.feature().value().getFeatures()), defaultFeature.value().getFeatures());
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        for (WeightedPlacedFeature candidate : features) {
            if (random.nextFloat() < candidate.chance()) {
                return candidate.place(level, chunkGenerator, random, origin);
            }
        }
        return defaultFeature.value().place(level, chunkGenerator, random, origin);
    }
}
