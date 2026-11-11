// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.phys.shapes.Shapes;
import net.minecraft.world.phys.shapes.BooleanOp;
import net.minecraft.world.phys.shapes.VoxelShape;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockState;
public enum SupportType {
    FULL, CENTER, RIGID;
    public boolean isSupporting(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        VoxelShape support = state.getBlockSupportShape(level, pos);
        return switch (this) {
            case FULL -> Block.isFaceFull(support, direction);
            case CENTER -> !Shapes.joinIsNotEmpty(support.getFaceShape(direction), Block.column(2.0, 0.0, 10.0), BooleanOp.ONLY_SECOND);
            case RIGID -> !Shapes.joinIsNotEmpty(support.getFaceShape(direction),
                Shapes.join(Shapes.block(), Block.column(12.0, 0.0, 16.0), BooleanOp.ONLY_FIRST), BooleanOp.ONLY_SECOND);
        };
    }
}
