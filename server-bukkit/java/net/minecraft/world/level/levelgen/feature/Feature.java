// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;

import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import org.basaltmc.bukkit.Unsupported;
import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderSet;
import net.minecraft.world.level.LevelWriter;
import net.minecraft.world.level.block.state.BlockState;
public interface Feature {
    public default boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        throw Unsupported.hit("net.minecraft.world.level.levelgen.feature.Feature.place");
    }
    Codec<Feature> DIRECT_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.Feature.class, "DIRECT_CODEC");
    Codec<Holder<Feature>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.Feature.class, "CODEC");
    Codec<HolderSet<Feature>> LIST_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.levelgen.feature.Feature.class, "LIST_CODEC");
    default MapCodec<? extends Feature> codec() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.Feature#codec");
    }
    default Stream<Holder<Feature>> getSubFeatures() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.Feature#getSubFeatures");
    }
    default void setBlock(LevelWriter level, BlockPos pos, BlockState blockState) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.Feature#setBlock");
    }
    default void safeSetBlock(WorldGenLevel level, BlockPos pos, BlockState state, Predicate<BlockState> canReplace) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.Feature#safeSetBlock");
    }
    default void markAboveForPostProcessing(WorldGenLevel level, BlockPos placePos) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.levelgen.feature.Feature#markAboveForPostProcessing");
    }
}
