// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.SignalGetter;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.redstone.Orientation;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public abstract class DiodeBlock extends HorizontalDirectionalBlock {
    protected DiodeBlock(int index) {
        super(index);
    }
    public static final BooleanProperty POWERED = BlockStateProperties.POWERED;
    protected DiodeBlock(BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    protected boolean canSurvive(BlockState state, LevelReader level, BlockPos pos) {
        return super.canSurvive(state, level, pos);
    }
    protected boolean canSurviveOn(LevelReader level, BlockPos neightborPos, BlockState neighborState) {
        return neighborState.isFaceSturdy(level, neightborPos, Direction.UP, SupportType.RIGID);
    }
    protected void tick(BlockState state, ServerLevel level, BlockPos pos, RandomSource random) {
        super.tick(state, level, pos, random);
    }
    protected void neighborChanged(BlockState state, Level level, BlockPos pos, Block block, Orientation orientation, boolean movedByPiston) {
        super.neighborChanged(state, level, pos, block, orientation, movedByPiston);
    }
    protected void checkTickOnNeighbor(Level level, BlockPos pos, BlockState state) {
        if (state.canSurvive(level, pos) && level instanceof net.minecraft.server.level.ServerLevel server
            && !server.getBlockTicks().hasScheduledTick(pos, this)) {
            boolean powered = state.getValue(POWERED);
            if (powered != shouldTurnOn(level, pos, state) && !isLocked(level, pos, state)) {
                server.scheduleTick(pos, this, getDelay(state));
            }
        }
    }
    public boolean isLocked(LevelReader level, BlockPos pos, BlockState state) {
        return getAlternateSignal(level, pos, state) > 0;
    }
    protected boolean shouldTurnOn(Level level, BlockPos pos, BlockState state) {
        return getInputSignal(level, pos, state) > 0;
    }
    protected int getInputSignal(Level level, BlockPos pos, BlockState state) {
        Direction facing = state.getValue(FACING);
        BlockPos behind = pos.relative(facing);
        BlockState behindState = level.getBlockState(behind);
        int power = level instanceof net.minecraft.server.level.ServerLevel server
            ? org.basaltmc.bukkit.WorldEnvNatives.signal(server.getWorld().getName(), behind.getX(), behind.getY(),
                behind.getZ(), facing.ordinal(), false) : 0;
        if (power < 15 && behindState.is(Blocks.REDSTONE_WIRE)) {
            power = Math.max(power, behindState.getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.POWER));
        }
        return power;
    }
    protected int getAlternateSignal(SignalGetter level, BlockPos pos, BlockState state) {
        Direction facing = state.getValue(FACING);
        int best = 0;
        for (Direction side : new Direction[] {facing.getClockWise(), facing.getCounterClockWise()}) {
            BlockPos at = pos.relative(side);
            BlockState sideState = level.getBlockState(at);
            boolean counts = sideInputDiodesOnly() ? isDiode(sideState) : sideState.isSignalSource();
            if (!counts) {
                continue;
            }
            if (sideState.is(Blocks.REDSTONE_BLOCK)) {
                return 15;
            }
            if (sideState.is(Blocks.REDSTONE_WIRE)) {
                best = Math.max(best, sideState.getValue(net.minecraft.world.level.block.state.properties.BlockStateProperties.POWER));
            } else if (level instanceof net.minecraft.server.level.ServerLevel server) {
                best = Math.max(best, org.basaltmc.bukkit.WorldEnvNatives.signal(server.getWorld().getName(), at.getX(),
                    at.getY(), at.getZ(), side.ordinal(), true));
            }
        }
        return best;
    }
    protected boolean isSignalSource(BlockState state) {
        return super.isSignalSource(state);
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
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public void setPlacedBy(Level level, BlockPos pos, BlockState state, LivingEntity by, ItemStack itemStack) {
        super.setPlacedBy(level, pos, state, by, itemStack);
    }
    protected void onPlace(BlockState state, Level level, BlockPos pos, BlockState oldState, boolean movedByPiston) {
        super.onPlace(state, level, pos, oldState, movedByPiston);
    }
    protected void affectNeighborsAfterRemoval(BlockState state, ServerLevel level, BlockPos pos, boolean movedByPiston) {
        super.affectNeighborsAfterRemoval(state, level, pos, movedByPiston);
    }
    protected void updateNeighborsInFront(Level level, BlockPos pos, BlockState state) {
        Direction facing = state.getValue(FACING);
        BlockPos front = pos.relative(facing.getOpposite());
        level.neighborChanged(front, this, null);
        level.updateNeighborsAtExceptFromFacing(front, this, facing, null);
    }
    protected boolean sideInputDiodesOnly() {
        return false;
    }
    protected int getOutputSignal(BlockGetter level, BlockPos pos, BlockState state) {
        return 15;
    }
    public static boolean isDiode(BlockState state) {
        return state.getBlock() instanceof DiodeBlock;
    }
    public boolean shouldPrioritize(BlockGetter level, BlockPos pos, BlockState state) {
        BlockState front = level.getBlockState(pos.relative(state.getValue(FACING).getOpposite()));
        return isDiode(front) && front.getValue(FACING) != state.getValue(FACING);
    }
    protected abstract int getDelay(BlockState state);
}
