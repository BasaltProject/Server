// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.levelgen.placement.FeaturePlacer;
import com.mojang.serialization.MapCodec;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
public record OverlayFeature(HolderSet<PlacedFeature> features) implements Feature {
    public static final MapCodec<OverlayFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.OverlayFeature.class, "CODEC");
    public MapCodec<OverlayFeature> codec() {
        return CODEC;
    }
    public Stream<Holder<Feature>> getSubFeatures() {
        return features.stream().flatMap(f -> f.value().getFeatures());
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        FeaturePlacer placer = new FeaturePlacer(level, chunkGenerator);
        boolean any = false;
        for (Holder<PlacedFeature> entry : features) {
            any |= placer.place(entry.value(), random, origin);
        }
        return any;
    }
}
