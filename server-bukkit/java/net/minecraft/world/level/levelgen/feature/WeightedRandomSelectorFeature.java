// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import java.util.Optional;
import com.mojang.serialization.MapCodec;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.util.random.WeightedList;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record WeightedRandomSelectorFeature(WeightedList<Holder<PlacedFeature>> features) implements Feature {
    public static final MapCodec<WeightedRandomSelectorFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.WeightedRandomSelectorFeature.class, "CODEC");
    public MapCodec<WeightedRandomSelectorFeature> codec() {
        return CODEC;
    }
    public Stream<Holder<Feature>> getSubFeatures() {
        return features.unwrap().stream().flatMap(w -> w.value().value().getFeatures());
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        Optional<Holder<PlacedFeature>> chosen = features.getRandom(random);
        return chosen.map(h -> h.value().place(level, chunkGenerator, random, origin)).orElse(false);
    }
}
