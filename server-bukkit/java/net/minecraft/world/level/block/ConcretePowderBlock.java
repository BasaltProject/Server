// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.item.FallingBlockEntity;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
public class ConcretePowderBlock extends FallingBlock {
    private Block concrete;
    private static boolean canSolidify(BlockState state) {
        return state.getFluidState().is(net.minecraft.tags.FluidTags.WATER);
    }
    private static boolean touchesLiquid(BlockGetter level, BlockPos pos) {
        BlockPos.MutableBlockPos cursor = pos.mutable();
        for (Direction direction : Direction.values()) {
            if (direction == Direction.DOWN && !canSolidify(level.getBlockState(cursor))) {
                continue;
            }
            cursor.setWithOffset(pos, direction);
            BlockState neighbour = level.getBlockState(cursor);
            if (canSolidify(neighbour) && !neighbour.isFaceSturdy(level, pos, direction.getOpposite())) {
                return true;
            }
        }
        return false;
    }
    public ConcretePowderBlock(Block concrete, BlockBehaviour.Properties properties) {
        super((int) 0);
        this.concrete = concrete;
    }
    public void onLand(Level level, BlockPos pos, BlockState state, BlockState replacedBlock, FallingBlockEntity entity) {
        if (canSolidify(replacedBlock) || touchesLiquid(level, pos)) {
            level.setBlockAndUpdate(pos, concrete.defaultBlockState());
        }
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    public int getDustColor(BlockState blockState, BlockGetter level, BlockPos pos) {
        return blockState.getMapColor(level, pos).col;
    }
}
