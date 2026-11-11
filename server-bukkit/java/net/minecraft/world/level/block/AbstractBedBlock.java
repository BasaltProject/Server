// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.world.phys.shapes.VoxelShape;
import java.util.Optional;
import java.util.OptionalDouble;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.resources.Identifier;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.attribute.BedRule;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.context.BlockPlaceContext;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.CollisionGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ScheduledTickAccess;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.StateDefinition;
import net.minecraft.world.level.block.state.properties.BedPart;
import net.minecraft.world.level.block.state.properties.BooleanProperty;
import net.minecraft.world.level.block.state.properties.EnumProperty;
import net.minecraft.world.level.pathfinder.PathComputationType;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;
public abstract class AbstractBedBlock extends HorizontalDirectionalBlock {
    private static Optional<Vec3> standAt(EntityType<?> type, CollisionGetter level, BlockPos pos, int[][] offsets, boolean checkDangerous) {
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int[] offset : offsets) {
            cursor.set(pos.getX() + offset[0], pos.getY(), pos.getZ() + offset[1]);
            Vec3 position = net.minecraft.world.entity.vehicle.DismountHelper.findSafeDismountLocation(type, level, cursor, checkDangerous);
            if (position != null) {
                return Optional.of(position);
            }
        }
        return Optional.empty();
    }
    public static final EnumProperty<BedPart> PART = net.minecraft.world.level.block.state.properties.BlockStateProperties.BED_PART;
    public static final BooleanProperty OCCUPIED = net.minecraft.world.level.block.state.properties.BlockStateProperties.OCCUPIED;
    public AbstractBedBlock(BlockBehaviour.Properties properties) {
        super((int) 0);
    }
    public static Direction getBedOrientation(BlockGetter level, BlockPos pos) {
        BlockState found = level.getBlockState(pos);
        return found.getBlock() instanceof AbstractBedBlock ? found.getValue(FACING) : null;
    }
    protected abstract EnvironmentAttribute<BedRule> getBedEnvironmentAttribute();
    protected abstract InteractionResult destroyOnUse(BlockState state, Level level, BlockPos pos, Player player);
    protected abstract void destroyOnLeave(Level level, BlockPos pos);
    public BedRule getBedRule(Level level, BlockPos pos) {
        return level.environmentAttributes().getValue(getBedEnvironmentAttribute(), pos);
    }
    public Identifier getSleptInBedStatType() {
        return net.minecraft.stats.Stats.SLEEP_IN_BED;
    }
    public OptionalDouble getSleepHeight(BlockState state, Level level, BlockPos pos) {
        if (!state.is(this)) {
            return OptionalDouble.empty();
        }
        VoxelShape bedShape = state.getShape(level, pos);
        if (bedShape.isEmpty()) {
            return OptionalDouble.empty();
        }
        return OptionalDouble.of(bedShape.max(Direction.Axis.Y));
    }
    protected InteractionResult useWithoutItem(BlockState state, Level level, BlockPos pos, Player player, BlockHitResult hitResult) {
        return super.useWithoutItem(state, level, pos, player, hitResult);
    }
    public void onStopSleeping(Level level, BlockPos pos) {
        if (getBedRule(level, pos).destroyOnLeave()) {
            destroyOnLeave(level, pos);
        }
    }
    protected BlockState updateShape(BlockState state, LevelReader level, ScheduledTickAccess ticks, BlockPos pos, Direction directionToNeighbour, BlockPos neighbourPos, BlockState neighbourState, RandomSource random) {
        return super.updateShape(state, level, ticks, pos, directionToNeighbour, neighbourPos, neighbourState, random);
    }
    public BlockState playerWillDestroy(Level level, BlockPos pos, BlockState state, Player player) {
        return super.playerWillDestroy(level, pos, state, player);
    }
    public BlockState getStateForPlacement(BlockPlaceContext context) {
        return super.getStateForPlacement(context);
    }
    public static Direction getConnectedDirection(BlockState state) {
        Direction facing = state.getValue(FACING);
        if (state.getValue(PART) == BedPart.HEAD) {
            return facing.getOpposite();
        }
        return facing;
    }
    public static DoubleBlockCombiner.BlockType getBlockType(BlockState state) {
        return state.getValue(PART) == BedPart.HEAD ? DoubleBlockCombiner.BlockType.FIRST : DoubleBlockCombiner.BlockType.SECOND;
    }
    public static Optional<Vec3> findStandUpPosition(EntityType<?> type, CollisionGetter level, BlockPos pos, Direction forward, float yaw) {
        Direction clockwise = forward.getClockWise();
        Direction side = clockwise;
        if (clockwise.isFacingAngle(yaw)) {
            side = clockwise.getOpposite();
        }
        boolean bunk = level.getBlockState(pos.below()).getBlock() instanceof AbstractBedBlock;
        int[][] around = {{side.getStepX(), side.getStepZ()},
            {side.getStepX() - forward.getStepX(), side.getStepZ() - forward.getStepZ()},
            {side.getStepX() - forward.getStepX() * 2, side.getStepZ() - forward.getStepZ() * 2},
            {-forward.getStepX() * 2, -forward.getStepZ() * 2},
            {-side.getStepX() - forward.getStepX() * 2, -side.getStepZ() - forward.getStepZ() * 2},
            {-side.getStepX() - forward.getStepX(), -side.getStepZ() - forward.getStepZ()},
            {-side.getStepX(), -side.getStepZ()},
            {-side.getStepX() + forward.getStepX(), -side.getStepZ() + forward.getStepZ()},
            {forward.getStepX(), forward.getStepZ()},
            {side.getStepX() + forward.getStepX(), side.getStepZ() + forward.getStepZ()}};
        int[][] above = {{0, 0}, {-forward.getStepX(), -forward.getStepZ()}};
        int[][] all = new int[around.length + above.length][];
        System.arraycopy(around, 0, all, 0, around.length);
        System.arraycopy(above, 0, all, around.length, above.length);
        BlockPos below = pos.below();
        for (boolean safe : new boolean[] {true, false}) {
            if (bunk) {
                Optional<Vec3> found = standAt(type, level, pos, around, safe);
                if (found.isEmpty()) {
                    found = standAt(type, level, below, around, safe);
                }
                if (found.isEmpty()) {
                    found = standAt(type, level, pos, above, safe);
                }
                if (found.isPresent()) {
                    return found;
                }
            } else {
                Optional<Vec3> found = standAt(type, level, pos, all, safe);
                if (found.isPresent()) {
                    return found;
                }
            }
        }
        return Optional.empty();
    }
    protected void createBlockStateDefinition(StateDefinition.Builder<Block, BlockState> builder) {
        super.createBlockStateDefinition(builder);
    }
    public void setPlacedBy(Level level, BlockPos pos, BlockState state, LivingEntity by, ItemStack itemStack) {
        super.setPlacedBy(level, pos, state, by, itemStack);
    }
    protected long getSeed(BlockState state, BlockPos pos) {
        return super.getSeed(state, pos);
    }
    protected boolean isPathfindable(BlockState state, PathComputationType type) {
        return super.isPathfindable(state, type);
    }
}
