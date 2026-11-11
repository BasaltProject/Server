// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import com.mojang.serialization.MapCodec;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record SimpleRandomSelectorFeature(HolderSet<PlacedFeature> features) implements Feature {
    public static final MapCodec<SimpleRandomSelectorFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SimpleRandomSelectorFeature.class, "CODEC");
    public MapCodec<SimpleRandomSelectorFeature> codec() {
        return CODEC;
    }
    public Stream<Holder<Feature>> getSubFeatures() {
        return features.stream().flatMap(f -> f.value().getFeatures());
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        PlacedFeature chosen = features.get(random.nextInt(features.size())).value();
        return chosen.place(level, chunkGenerator, random, origin);
    }
}
