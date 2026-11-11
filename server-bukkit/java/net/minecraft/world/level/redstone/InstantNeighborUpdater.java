// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public class InstantNeighborUpdater implements NeighborUpdater {
    private final Level level;
    public InstantNeighborUpdater(Level level) {
        this.level = level;
    }
    public void shapeUpdate(Direction direction, BlockState neighborState, BlockPos pos, BlockPos neighborPos, int updateFlags, int updateLimit) {
        NeighborUpdater.executeShapeUpdate(level, direction, pos, neighborPos, neighborState, updateFlags, updateLimit - 1);
    }
    public void neighborChanged(BlockPos pos, Block changedBlock, Orientation orientation) {
        neighborChanged(level.getBlockState(pos), pos, changedBlock, orientation, false);
    }
    public void neighborChanged(BlockState state, BlockPos pos, Block changedBlock, Orientation orientation, boolean movedByPiston) {
        NeighborUpdater.executeUpdate(level, state, pos, changedBlock, orientation, movedByPiston);
    }
}
