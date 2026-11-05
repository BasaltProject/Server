// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
public interface SignalGetter extends BlockGetter {
    Direction[] DIRECTIONS = Direction.values();
    default int getDirectSignal(BlockPos pos, Direction direction) {
        return getBlockState(pos).getDirectSignal(this, pos, direction);
    }
    default int getDirectSignalTo(BlockPos pos) {
        int best = 0;
        for (Direction side : DIRECTIONS) {
            best = Math.max(best, getDirectSignal(pos.relative(side), side));
            if (best >= 15) {
                break;
            }
        }
        return best;
    }
    default int getControlInputSignal(BlockPos pos, Direction direction, boolean onlyDiodes) {
        BlockState state = getBlockState(pos);
        if (onlyDiodes) {
            return net.minecraft.world.level.block.DiodeBlock.isDiode(state) ? getDirectSignal(pos, direction) : 0;
        }
        if (state.is(net.minecraft.world.level.block.Blocks.REDSTONE_BLOCK)) {
            return 15;
        }
        if (state.is(net.minecraft.world.level.block.Blocks.REDSTONE_WIRE)) {
            return state.getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.POWER);
        }
        return state.isSignalSource() ? getDirectSignal(pos, direction) : 0;
    }
    default boolean hasSignal(BlockPos pos, Direction direction) {
        return getSignal(pos, direction) > 0;
    }
    default int getSignal(BlockPos pos, Direction direction) {
        BlockState state = getBlockState(pos);
        int own = state.getSignal(this, pos, direction);
        return state.isRedstoneConductor(this, pos) ? Math.max(own, getDirectSignalTo(pos)) : own;
    }
    default int getBestOwnOrNeighbourSignal(BlockPos pos) {
        BlockState state = getBlockState(pos);
        return Math.max(getBestNeighborSignal(pos), state.isSignalSource() ? state.getOwnSignal(this, pos) : 0);
    }
    default boolean hasNeighborSignal(BlockPos blockPos) {
        for (Direction side : DIRECTIONS) {
            if (getSignal(blockPos.relative(side), side) > 0) {
                return true;
            }
        }
        return false;
    }
    default int getBestNeighborSignal(BlockPos pos) {
        int best = 0;
        for (Direction side : DIRECTIONS) {
            best = Math.max(best, getSignal(pos.relative(side), side));
            if (best >= 15) {
                return 15;
            }
        }
        return best;
    }
}
