// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.redstone;
import net.minecraft.CrashReport;
import net.minecraft.CrashReportCategory;
import net.minecraft.ReportedException;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;
public interface NeighborUpdater {
    Direction[] UPDATE_ORDER = new Direction[] {Direction.WEST, Direction.EAST, Direction.DOWN, Direction.UP, Direction.NORTH, Direction.SOUTH};
    void shapeUpdate(Direction direction, BlockState neighborState, BlockPos pos, BlockPos neighborPos, int updateFlags, int updateLimit);
    void neighborChanged(BlockPos pos, Block changedBlock, Orientation orientation);
    void neighborChanged(BlockState state, BlockPos pos, Block changedBlock, Orientation orientation, boolean movedByPiston);
    default void updateNeighborsAtExceptFromFacing(BlockPos pos, Block block, Direction skipDirection, Orientation orientation) {
        for (Direction side : UPDATE_ORDER) {
            if (side != skipDirection) {
                neighborChanged(pos.relative(side), block, null);
            }
        }
    }
    static void executeShapeUpdate(LevelAccessor level, Direction direction, BlockPos pos, BlockPos neighborPos, BlockState neighborState, int updateFlags, int updateLimit) {
        BlockState current = level.getBlockState(pos);
        if ((updateFlags & 128) != 0 && current.is(Blocks.REDSTONE_WIRE)) {
            return;
        }
        try {
            BlockState updated = current.updateShape(level, level, pos, direction, neighborPos, neighborState, level.getRandom());
            Block.updateOrDestroy(current, updated, level, pos, updateFlags, updateLimit);
        } catch (Throwable failure) {
            CrashReport report = CrashReport.forThrowable(failure, "Exception while updating neighbour shapes");
            CrashReportCategory.populateBlockDetails(report.addCategory("Block being updated"), level, pos, current);
            CrashReportCategory.populateBlockDetails(report.addCategory("Neighbor block"), level, neighborPos, neighborState);
            throw new ReportedException(report);
        }
    }
    static void executeUpdate(Level level, BlockState state, BlockPos pos, Block changedBlock, Orientation orientation, boolean movedByPiston) {
        try {
            state.handleNeighborChanged(level, pos, changedBlock, orientation, movedByPiston);
        } catch (Throwable failure) {
            CrashReport report = CrashReport.forThrowable(failure, "Exception while updating neighbours");
            CrashReportCategory.populateBlockDetails(report.addCategory("Block being updated"), level, pos, state);
            throw new ReportedException(report);
        }
    }
}
