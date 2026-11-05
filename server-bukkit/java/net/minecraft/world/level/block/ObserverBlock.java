// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
public class ObserverBlock extends DirectionalBlock {
    protected ObserverBlock(int index) {
        super(index);
    }
    public static final BooleanProperty POWERED = BlockStateProperties.POWERED;
    public ObserverBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    protected BlockState rotate(BlockState state, Rotation rotation) {
        return super.rotate(state, rotation);
    }
    protected BlockState mirror(BlockState state, Mirror mirror) {
        return super.mirror(state, mirror);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    protected void updateNeighborsInFront(Level level, BlockPos pos, BlockState state) {
        Direction behind = state.getValue(FACING).getOpposite();
        BlockPos target = pos.relative(behind);
        Orientation orientation = net.minecraft.world.level.redstone.ExperimentalRedstoneUtils.initialOrientation(level, behind, null);
        level.neighborChanged(target, this, orientation);
        level.updateNeighborsAtExceptFromFacing(target, this, behind.getOpposite(), orientation);
    }
    protected boolean isSignalSource(BlockState state) {
        return super.isSignalSource(state);
    }
    protected boolean shouldRedstoneWireConnectTo(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return super.shouldRedstoneWireConnectTo(state, level, pos, direction);
    }
    protected int ownSignal(BlockState state, BlockGetter level, BlockPos pos) {
        return super.ownSignal(state, level, pos);
    }
    protected int getDirectSignal(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return super.getDirectSignal(state, level, pos, direction);
    }
    protected int getSignal(BlockState state, BlockGetter level, BlockPos pos, Direction direction) {
        return super.getSignal(state, level, pos, direction);
    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    protected void affectNeighborsAfterRemoval(BlockState state, ServerLevel level, BlockPos pos, boolean movedByPiston) {
        super.affectNeighborsAfterRemoval(state, level, pos, movedByPiston);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
}
