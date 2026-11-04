// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;

public class WallHangingSignBlock extends SignBlock {
    protected WallHangingSignBlock(int index) {
        super(index);
    }

    /** A wall hanging sign needs a solid block at one of its two ends along the wall. */
    public boolean canPlace(BlockState state, LevelReader level, BlockPos pos) {
        Direction facing = state.getValue(HorizontalDirectionalBlock.FACING);
        Direction right = facing.getClockWise();
        Direction left = facing.getCounterClockWise();
        return attachedTo(level, pos.relative(right), left) || attachedTo(level, pos.relative(left), right);
    }

    private static boolean attachedTo(LevelReader level, BlockPos at, Direction face) {
        return level.getBlockState(at).isFaceSturdy(level, at, face, SupportType.FULL);
    }
}
