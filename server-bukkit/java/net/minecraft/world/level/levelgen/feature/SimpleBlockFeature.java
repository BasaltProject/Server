// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.BlockState;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.feature.stateproviders.BlockStateProvider;
public record SimpleBlockFeature(Holder<BlockStateProvider> toPlace, boolean scheduleTick) implements Feature {
    public static final MapCodec<SimpleBlockFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SimpleBlockFeature.class, "CODEC");
    public SimpleBlockFeature(Holder<BlockStateProvider> toPlace) {
        this(toPlace, false);
    }
    public SimpleBlockFeature(BlockStateProvider toPlace) {
        this(Holder.direct(toPlace));
    }
    public MapCodec<SimpleBlockFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        BlockState chosen = toPlace.value().getOptionalState(level, random, origin);
        if (chosen == null || !chosen.canSurvive(level, origin)) {
            return false;
        }
        if (chosen.getBlock() instanceof net.minecraft.world.level.block.DoublePlantBlock) {
            BlockState upper = level.getBlockState(origin.above());
            if (!upper.isAir() && (!java.util.Objects.equals(chosen.getFluidState(), upper.getFluidState()) || !upper.canBeReplaced())) {
                return false;
            }
            net.minecraft.world.level.block.DoublePlantBlock.placeAt(level, chosen, origin, 2);
        } else if (chosen.getBlock() instanceof net.minecraft.world.level.block.MossyCarpetBlock) {
            net.minecraft.world.level.block.MossyCarpetBlock.placeAt(level, origin, level.getRandom(), 2);
        } else {
            level.setBlock(origin, chosen, 2);
        }
        if (scheduleTick) {
            level.scheduleTick(origin, level.getBlockState(origin).getBlock(), 1);
        }
        return true;
    }
}
