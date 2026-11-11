// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.feature;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.Direction;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderSet;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.material.FluidState;
public record SpringFeature(FluidState state, boolean requiresBlockBelow, int rockCount, int holeCount, HolderSet<Block> validBlocks) implements Feature {
    public static final MapCodec<SpringFeature> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.feature.SpringFeature.class, "CODEC");
    public MapCodec<SpringFeature> codec() {
        return CODEC;
    }
    public boolean place(WorldGenLevel level, ChunkGenerator chunkGenerator, RandomSource random, BlockPos origin) {
        if (!level.getBlockState(origin.above()).is(validBlocks)) {
            return false;
        }
        if (requiresBlockBelow && !level.getBlockState(origin.below()).is(validBlocks)) {
            return false;
        }
        BlockState here = level.getBlockState(origin);
        if (!here.isAir() && !here.is(validBlocks)) {
            return false;
        }
        int rocks = 0;
        int holes = 0;
        for (Direction side : new Direction[] {Direction.WEST, Direction.EAST, Direction.NORTH, Direction.SOUTH, Direction.DOWN}) {
            BlockPos next = origin.relative(side);
            if (level.getBlockState(next).is(validBlocks)) {
                rocks++;
            }
            if (level.isEmptyBlock(next)) {
                holes++;
            }
        }
        if (rocks == rockCount && holes == holeCount) {
            level.setBlock(origin, state.createLegacyBlock(), 2);
            level.scheduleTick(origin, state.getType(), 0);
            return true;
        }
        return false;
    }
}
