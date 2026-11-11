// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record RandomBooleanSelectorFeature(Holder<PlacedFeature> featureTrue, Holder<PlacedFeature> featureFalse) implements Feature {
    public static final MapCodec<RandomBooleanSelectorFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.RandomBooleanSelectorFeature.class, "CODEC");
    public MapCodec<RandomBooleanSelectorFeature> codec() {
        return CODEC;
    }
    public Stream<Holder<Feature>> getSubFeatures() {
        return Stream.concat(featureTrue.value().getFeatures(), featureFalse.value().getFeatures());
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        boolean flip = random.nextBoolean();
        return (flip ? featureTrue : featureFalse).value().place(level, chunkGenerator, random, origin);
    }
}
