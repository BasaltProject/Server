// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.placement;

import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.serialization.Codec;
import java.util.stream.Stream;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.levelgen.feature.Feature;
public record PlacedFeature(Holder feature, List placement) {
    public boolean place(WorldGenLevel level, ChunkGenerator generator, RandomSource random, BlockPos origin) {
        return feature instanceof Holder<?> holder && holder.unwrapKey().isPresent()
            && org.basaltmc.bukkit.facade.NativeFeatures.place(level, holder.unwrapKey().get().identifier(), origin, random.nextLong());
    }
    public static final Codec<PlacedFeature> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.placement.PlacedFeature.class, "DIRECT_CODEC");
    public static final Codec<Holder<PlacedFeature>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.placement.PlacedFeature.class, "CODEC");
    public static final Codec<HolderSet<PlacedFeature>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.placement.PlacedFeature.class, "LIST_CODEC");
    public static final Codec<List<HolderSet<PlacedFeature>>> LIST_OF_LISTS_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.placement.PlacedFeature.class, "LIST_OF_LISTS_CODEC");
    public Stream<Holder<Feature>> getFeatures() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.placement.PlacedFeature#getFeatures");
    }
}
